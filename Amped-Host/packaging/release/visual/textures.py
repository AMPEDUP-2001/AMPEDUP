"""Generate opaque Xbox texture overrides from the original materials."""

from io import BytesIO
import struct

import numpy as np
from PIL import Image

BC1 = 0x0C


def decode_xpr(payload, max_dimension=256):
    magic, total, data_start, common, data, _, fmt, _, end = struct.unpack_from("<4s8I", payload)
    if (magic, total, data_start, common, data, end) != (
        b"XPR0", len(payload), 36, 0x40001, 0, 0xFFFFFFFF
    ):
        raise ValueError("Expected one original Xbox texture")
    width, height = 1 << (fmt >> 20 & 15), 1 << (fmt >> 24 & 15)
    if (fmt >> 8 & 255) != BC1 or not (16 <= width <= max_dimension and 16 <= height <= max_dimension):
        raise ValueError("Expected a BC1 texture within the requested dimensions")
    level0 = payload[data_start:data_start + width * height // 2]
    if len(level0) != width * height // 2:
        raise ValueError("Truncated BC1 level")
    dds = struct.pack(
        "<4s31I", b"DDS ", 124, 0x81007, height, width, len(level0), 0, 0, *([0] * 11),
        32, 4, int.from_bytes(b"DXT1", "little"), 0, 0, 0, 0, 0, 0x1000, 0, 0, 0, 0,
    )
    with Image.open(BytesIO(dds + level0)) as image:
        decoded = image.convert("RGBA")
    if decoded.getchannel("A").getextrema() != (255, 255):
        raise ValueError("Cutout texture requires an alpha-aware upscale")
    return level0, decoded


def wrap_resize(image):
    width, height = image.size
    border = 8
    tiled = Image.new("RGBA", (width + 2 * border, height + 2 * border))
    for dy in (-height, 0, height):
        for dx in (-width, 0, width):
            tiled.paste(image, (border + dx, border + dy))
    scaled = tiled.resize((2 * tiled.width, 2 * tiled.height), Image.Resampling.LANCZOS)
    return scaled.crop((2 * border, 2 * border, 2 * (border + width), 2 * (border + height)))


def overlap_edges(pixels, axis, period, overlap):
    pixels = np.moveaxis(pixels, axis, 0)
    tile = pixels[:period].copy()
    t = np.linspace(0, np.pi, overlap, dtype=np.float32)
    t = ((1 - np.cos(t)) / 2).reshape((overlap,) + (1,) * (pixels.ndim - 1))
    tile[:overlap] = pixels[period:] * (1 - t) + pixels[:overlap] * t
    return np.moveaxis(tile, 0, axis)


def tile_detail(path, size, rotation, contrast):
    width, height = size
    pad_x, pad_y = max(8, width // 10), max(8, height // 10)
    with Image.open(path) as source:
        raster = source.convert("L").rotate(rotation, expand=True)
        channel = np.asarray(raster.resize((width + pad_x, height + pad_y), Image.Resampling.LANCZOS),
                             dtype=np.float32)
    tile = overlap_edges(overlap_edges(channel, 0, height, pad_y), 1, width, pad_x)
    smooth = tile
    # A wrap-aware low-pass keeps the new scratches and brushing separate
    # from long-range gradients or baked illumination in the source art.
    for step in (1, 2, 4, 8):
        for axis in (0, 1):
            smooth = (np.roll(smooth, step, axis=axis) + 2 * smooth +
                      np.roll(smooth, -step, axis=axis)) * 0.25
    high = tile - smooth
    normalized = np.clip(high * min(3.0, contrast / max(float(high.std()), 0.01)), -20, 20)
    return normalized - normalized.mean()


def replace_texture(original, detail):
    original_middle = wrap_resize(original)
    reference = np.asarray(wrap_resize(original_middle), dtype=np.float32)[..., :3]
    enhanced = np.clip(np.rint(reference + detail[..., None]), 0, 255).astype(np.uint8)
    top = Image.fromarray(enhanced).convert("RGBA")
    width, height = top.size
    # The closer mip inherits the new detail, and the blend limits any jump
    # into the game's original mip chain when the camera moves away.
    middle = Image.blend(top.resize((width // 2, height // 2), Image.Resampling.BOX),
                         original_middle, 0.18)
    return top, middle


def groomed(original, artwork):
    width, height = original.size
    if width != height:
        raise ValueError("Expected a square snow material")
    period, overlap = 4 * width, width // 2
    with Image.open(artwork) as source:
        luminance = np.asarray(source.convert("L").resize((period + overlap,) * 2, Image.Resampling.LANCZOS),
                               dtype=np.float32)
    tile = overlap_edges(overlap_edges(luminance, 0, period, overlap), 1, period, overlap)
    rgb = np.asarray(original, dtype=np.float32)[..., :3]
    # Match the original terrain's brightness, tint and approximate contrast.
    target_contrast = float(rgb.mean(axis=2).std())
    detail = (tile - tile.mean()) * min(1.5, target_contrast / tile.std())
    detail = np.clip(detail, -30, 30)
    level4 = Image.fromarray(np.clip(np.rint(rgb.mean((0, 1)) + detail[..., None]), 0, 255).astype(np.uint8))
    level4 = level4.convert("RGBA")
    level2 = level4.resize((2 * width, 2 * height), Image.Resampling.BOX)
    level2 = Image.blend(level2, wrap_resize(original), 0.32)
    return level4, level2

"""Generate opaque Xbox texture overrides from the original materials."""

from io import BytesIO
import struct

import numpy as np
from PIL import Image

BC1 = 0x0C
BC2 = 0x0E


def decode_xpr(payload, max_dimension=256, *, color_format=BC1):
    magic, total, data_start, common, data, _, fmt, _, end = struct.unpack_from("<4s8I", payload)
    if (magic, total, data_start, common, data, end) != (
        b"XPR0", len(payload), 36, 0x40001, 0, 0xFFFFFFFF
    ):
        raise ValueError("Expected one original Xbox texture")
    width, height = 1 << (fmt >> 20 & 15), 1 << (fmt >> 24 & 15)
    if color_format not in (BC1, BC2) or (fmt >> 8 & 255) != color_format:
        raise ValueError("Unexpected Xbox texture format")
    if not (16 <= width <= max_dimension and 16 <= height <= max_dimension):
        raise ValueError("Texture dimensions exceed the requested bounds")
    size = width * height // (2 if color_format == BC1 else 1)
    level0 = payload[data_start:data_start + size]
    if len(level0) != size:
        raise ValueError("Truncated compressed texture")
    fourcc = b"DXT1" if color_format == BC1 else b"DXT3"
    dds = struct.pack(
        "<4s31I", b"DDS ", 124, 0x81007, height, width, len(level0), 0, 0, *([0] * 11),
        32, 4, int.from_bytes(fourcc, "little"), 0, 0, 0, 0, 0, 0x1000, 0, 0, 0, 0,
    )
    with Image.open(BytesIO(dds + level0)) as image:
        decoded = image.convert("RGBA")
    if color_format == BC1 and decoded.getchannel("A").getextrema() != (255, 255):
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


def charcoal_levels(original):
    middle = wrap_resize(original)
    base = np.asarray(wrap_resize(middle), dtype=np.int16)[..., :3]
    luma = np.asarray(original.convert("L"), dtype=np.float32)
    # Keep the coating's large-scale mottling. The fine component comes from
    # the same disc tile so the material also suits its other metal surfaces.
    smooth = luma
    for axis in (0, 1):
        smooth = (np.roll(smooth, 1, axis) + 2 * smooth + np.roll(smooth, -1, axis)) * 0.25
    grain = luma - smooth
    grain = grain + 0.55 * np.roll(grain.T, (17, 29), axis=(0, 1))
    grain = np.clip(grain * (5.5 / max(float(grain.std()), 0.01)), -15, 15)
    grain = np.tile(grain, (4, 4))
    delta = np.rint(np.clip(grain, -base.min(axis=2), 255 - base.max(axis=2))).astype(np.int16)
    top = Image.fromarray((base + delta[..., None]).astype(np.uint8)).convert("RGBA")
    return top, Image.blend(top.resize(middle.size, Image.Resampling.BOX), middle, 0.18)


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


def replace_close_texture(original, detail):
    rgb = np.asarray(wrap_resize(original), dtype=np.float32)[..., :3]
    # Keep the same brightness adjustment on all channels, including saturated paint.
    delta = np.clip(detail, -rgb.min(axis=2), 255 - rgb.max(axis=2))
    return Image.fromarray(np.rint(rgb + delta[..., None]).astype(np.uint8)).convert("RGBA")


def painted_levels(original, artwork):
    middle = wrap_resize(original)
    top = wrap_resize(middle)
    rgb = np.asarray(top, dtype=np.float32)[..., :3]
    # The long side of the funbox follows U. Align the fibres with the board
    # and ease the grain off the dark edge and the original pale scuffs.
    red = np.clip((rgb[..., 0] - rgb[..., 1] - 36) / 77, 0, 1)
    grain = tile_detail(artwork, top.size, 90, 5.0) * red
    delta = np.rint(np.clip(grain, -rgb.min(axis=2), 255 - rgb.max(axis=2)))
    painted = Image.fromarray((rgb + delta[..., None]).astype(np.uint8)).convert("RGBA")
    transition = Image.blend(painted.resize(middle.size, Image.Resampling.BOX), middle, 0.35)
    return painted, transition


def ramp_snow_level(middle, artwork):
    base = wrap_resize(middle)
    rgb = np.asarray(base, dtype=np.float32)[..., :3]
    # The original sidewall is pale snow. Ease grain over its blue-tinted patches.
    low, high = rgb.min(axis=2), rgb.max(axis=2)
    snow = np.clip(1 - (high - low) / 26, 0, 1)
    grain = tile_detail(artwork, base.size, 90, 9.0)
    delta = np.rint(np.clip(snow * grain, -low, 255 - high))
    return Image.fromarray((rgb + delta[..., None]).astype(np.uint8)).convert("RGBA")


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

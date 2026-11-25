"""Generate the close BC2 mip for the optional pine foliage materials."""
import numpy as np
from PIL import Image

def needle_detail(path):
    with Image.open(path) as src:
        tile = np.asarray(src.convert("L"), dtype=np.float32)
    if tile.shape != (256, 256):
        raise ValueError("Expected a 256-pixel pine-needle swatch")
    blur = tile
    for axis in (0, 1):
        for step in (1, 2):
            blur = (np.roll(blur, step, axis=axis) + 2 * blur + np.roll(blur, -step, axis=axis)) * .25
    high = tile - blur
    return np.clip(high * min(2.5, 14.0 / max(float(high.std()), .01)), -28, 28)

def double(channel, resampling):
    image = Image.fromarray(channel.astype(np.float32, copy=False))
    return np.asarray(image.resize((2 * image.width, 2 * image.height), resampling))

def foliage_level(original, detail=None):
    rgba = np.asarray(original, dtype=np.float32)
    alpha = rgba[..., 3]
    if alpha.min() != 0 or alpha.max() != 255:
        raise ValueError("Expected a foliage cutout")
    weight = double(alpha, Image.Resampling.BILINEAR)
    cubic_weight = double(alpha, Image.Resampling.BICUBIC)
    # Retain both sides of the original alpha comparisons at 08 and 80 hex.
    linear = np.rint(weight)
    bounds = np.array([0, 8, 9, 128, 129, 256])
    bucket = np.searchsorted(bounds, linear, side="right") - 1
    mask = np.clip(np.rint(linear + .75 * (cubic_weight - linear)),
                   bounds[bucket], bounds[bucket + 1] - 1).astype(np.uint8)
    channels = []
    for channel in rgba.transpose(2, 0, 1)[:3]:
        associated = channel * alpha
        coarse = np.divide(double(associated, Image.Resampling.BILINEAR), weight,
                           out=double(channel, Image.Resampling.BILINEAR).copy(), where=weight > 0)
        fine = np.divide(double(associated, Image.Resampling.BICUBIC), cubic_weight,
                         out=coarse.copy(), where=cubic_weight > 64)
        channels.append(coarse + np.clip(fine - coarse, -12, 12))
    rgb = np.clip(np.rint(np.stack(channels, axis=-1)), 0, 255)
    if detail is not None:
        r, g, b = rgb.transpose(2, 0, 1)
        # Needle relief belongs to green foliage. Keep warm stems and snow.
        green = np.clip((g - b) / 24, 0, 1) * np.clip((g - r + 12) / 22, 0, 1)
        weight = green * np.clip(mask.astype(np.float32) / 128, 0, 1)
        delta = np.clip(weight * detail, -rgb.min(axis=-1), 255 - rgb.max(axis=-1))
        rgb = np.rint(rgb + delta[..., None])
    return Image.fromarray(np.dstack((rgb.astype(np.uint8), mask)))

"""Preserve the disc snow's low-frequency shading while placing grain between its texels."""
import numpy as np
from PIL import Image
from textures import tile_detail, wrap_resize


def wind_powder_levels(original, artwork, rotation=0, contrast=11):
    middle = wrap_resize(original)
    base = np.asarray(wrap_resize(middle), dtype=np.float32)[..., :3]
    grain = tile_detail(artwork, middle.size, rotation, contrast)
    grain = np.tile(grain, (2, 2))
    # Remove the portion of the new grain that would change each disc texel's
    # average. Reconstruct this slow component smoothly across wrap boundaries.
    h, w = original.height, original.width
    coarse = grain.reshape(h, 4, w, 4).mean(axis=(1, 3))
    border = 4
    padded = Image.fromarray(np.pad(coarse, border, mode="wrap"))
    low = padded.resize((4 * padded.width, 4 * padded.height), Image.Resampling.BICUBIC)
    low = np.asarray(low)[4 * border:4 * (border + h), 4 * border:4 * (border + w)]
    grain = np.clip((grain - low) * 1.15, -20, 20)
    delta = np.rint(np.clip(grain, -base.min(axis=2), 255 - base.max(axis=2)))
    top = Image.fromarray((base + delta[..., None]).astype(np.uint8)).convert("RGBA")
    middle = Image.blend(top.resize(middle.size, Image.Resampling.BOX), middle, .18)
    return top, middle

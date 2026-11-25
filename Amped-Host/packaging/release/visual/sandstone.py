"""Add sandstone detail to the Gunny's Gulch cliff textures."""
import struct

import numpy as np
from PIL import Image

from inspect_directory import directory
from textures import BC1, decode_xpr, tile_detail, wrap_resize

CLIFFS = (("gp_redrock_cliff.xpr", 1), ("rock_redstone.xpr", 2), ("rock_redstone_cliff.xpr", 2))

def cliff_levels(original, swatch, added):
    middle = wrap_resize(original)
    base = wrap_resize(middle) if added == 2 else middle
    rgb = np.asarray(base, dtype=np.float32)[..., :3]
    detail = tile_detail(swatch, base.size, 0, 13.0)
    # The source contains snow caps and deep crevices. Use the warm stone's
    # own colour as a mask, with a soft transition where snow meets rock.
    stone = np.clip((rgb[..., 0] - rgb[..., 2] - 8) / 36, 0, 1)
    delta = np.rint(np.clip(stone * detail, -rgb.min(axis=2), 255 - rgb.max(axis=2)))
    top = Image.fromarray((rgb + delta[..., None]).astype(np.uint8)).convert("RGBA")
    if added == 1:
        return (top,)
    return top, Image.blend(top.resize(middle.size, Image.Resampling.BOX), middle, .18)

def append_cliffs(pack, archive, swatch):
    originals = {name: (start, size) for name, start, size in directory(archive)}
    for name, added in CLIFFS:
        start, size = originals[name]
        key, original = decode_xpr(archive[start:start + size], max_dimension=512)
        levels = cliff_levels(original, swatch, added)
        if name == "gp_redrock_cliff.xpr":
            levels = (close_cliff(levels[0], swatch), *levels)
        pack.extend(struct.pack("<4I", *original.size, BC1, len(levels)))
        pack.extend(key)
        for level in levels:
            pack.extend(level.tobytes("raw", "BGRA"))


def close_cliff(middle, swatch):
    rgb = np.asarray(wrap_resize(middle), dtype=np.float32)[..., :3]
    # Repeat only the finest grain. The authored 1024 mip contains the larger beds.
    micro = tile_detail(swatch, (512, 512), 0, 14)
    smooth = micro
    for axis in (0, 1):
        smooth = (np.roll(smooth, 2, axis) + 2 * smooth + np.roll(smooth, -2, axis)) * .25
    fine = np.tile(np.clip((micro - smooth) * 1.75, -18, 18), (4, 4))
    stone = np.clip((rgb[..., 0] - rgb[..., 2] - 8) / 36, 0, 1)
    delta = np.rint(np.clip(fine * stone, -rgb.min(axis=2), 255 - rgb.max(axis=2)))
    return Image.fromarray((rgb + delta[..., None]).astype(np.uint8)).convert("RGBA")

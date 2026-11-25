"""Bake the optional close level for the European disc's woven safety fences."""
from hashlib import sha256
import struct

import numpy as np
from PIL import Image

from inspect_directory import directory
from textures import BC2, decode_xpr


NETS = (
    ("BR_Crest", "fence_blue.xpr", "dc65f96d1b3639627ecda29d374dc2be0e25dbca2566229b42abc96a74e7af76"),
    ("BR_Crest", "fence_orange.xpr", "3e14153ffaa729b68e56f11dfa8715457595eaea30f873952cf6f5bf093e714d"),
    ("SS_JibFest", "mm_fence_orangenet.xpr", "51112a31115116bd71cca2daf0a340524811526d5fcd8b07debec9ba94e28744"),
)


def net_level(original):
    rgba = np.asarray(original, dtype=np.float32)
    height, width = rgba.shape[:2]

    def double(channel):
        border = Image.fromarray(np.pad(channel, 1, mode="wrap"))
        scaled = border.resize((2 * width + 4, 2 * height + 4), Image.Resampling.BILINEAR)
        return np.asarray(scaled)[2:-2, 2:-2]

    alpha = rgba[..., 3]
    coverage = double(alpha)
    channels = []
    for channel in rgba.transpose(2, 0, 1)[:3]:
        channels.append(np.divide(double(channel * alpha), coverage,
                                  out=double(channel).copy(), where=coverage > 0))
    rgb = np.stack(channels, axis=2)
    y, x = np.indices(coverage.shape, dtype=np.float32)
    # Integral repeats keep the twill continuous across tile boundaries.
    phase = 2 * np.pi * (x / 8 + y / 8)
    weave = (9 * np.cos(phase) + 4 * np.cos(2 * np.pi * (x / 16 - y / 4))) * (coverage / 255)**2
    room = np.minimum(rgb, 255 - rgb)
    enhanced = rgb + weave[..., None] * np.clip(room / 16, 0, 1)
    pixels = np.dstack((np.clip(np.rint(enhanced), 0, 255), np.rint(coverage)))
    return Image.fromarray(pixels.astype(np.uint8))


def append_nets(pack, terrain):
    current_archive = None
    for archive, name, expected in NETS:
        if archive != current_archive:
            data = (terrain / (archive + ".img")).read_bytes()
            entries = {n: (o, s) for n, o, s in directory(data)}
            current_archive = archive
        offset, size = entries[name]
        key, original = decode_xpr(data[offset:offset + size], color_format=BC2)
        if sha256(key).hexdigest() != expected:
            raise ValueError(f"The safety fence {name} differs from the tested disc texture")
        pack.extend(struct.pack("<4I", *original.size, BC2, 1))
        pack.extend(key)
        pack.extend(net_level(original).tobytes("raw", "BGRA"))

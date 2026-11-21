"""Derive release visual assets from a verified European retail disc."""

import argparse
from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path
import struct
import sys

import numpy as np

from bevel_posts import bevel
from refine_rail import refine
from round_funbox import round_lip
from textures import BC1, decode_xpr, groomed, replace_texture, tile_detail

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / "research/vrtdrive"))
from inspect_directory import directory

COURSE = "Resource/Terrain/SS_EgoTrip"
GROOMED = "ter_snow_groomed_tracks_5x5.xpr"
# Order is part of the pack format. The last four entries also identify
# the original materials for matching the corresponding rail draws.
DETAILS = (
    ("ter_snow_packed_4x4.xpr", "packed-snow", 6.0, 0),
    ("ter_snow_powder_rough_3x3.xpr", "rough-powder", 6.5, 0),
    ("pinebark.xpr", "pine-bark", 7.5, 0),
    ("mm_rail_metalpole.xpr", "bare-steel", 7.5, 0),
    ("mm_rail_metalpole_red.xpr", "red-enamel", 6.0, 0),
    ("mm_funboxes_silvertop.xpr", "brushed-silver", 6.5, 0),
    ("mm_funboxes_metaledge.xpr", "bare-steel", 5.5, 90),
)
PACK = "snow-summit-visible.ahd"
PACK_SHA = "e14a1788286718fb2fbb2288b8acd860d172cf9e13efc16c26ce86a2bcbe2467"

# Retail archive locations for vertex data and its triangle strip, followed
# by the strip's vertex base, the material, the draw identity and the refiner.
MESHES = (
    (1809114, 88, 1811998, 237, 0, 1,
     "bdfe78d915face0727185c0d91cc4cca83e0dc7d77c3497e771a5f8548309444", refine),
    (1811226, 32, 1812472, 46, 88, 2,
     "be045b402851b763ffd863936acc7ea1b2d97336028bcf637b80860c2c370be3", bevel),
    (1812664, 220, 1818428, 360, 0, 4,
     "092ae60dabc6863f20cb961b6c2ffb884d454752f56d245613908945c8587f8e", round_lip),
)


@dataclass
class Draw:
    vertices: np.ndarray
    indices: np.ndarray
    material: int
    identity: str
    slots = (0, 1, 2)

    def positions(self):
        return self.vertices[:, 0, :3]


def read_verified(path, expected_sha):
    data = path.read_bytes()
    if sha256(data).hexdigest() != expected_sha:
        raise ValueError(f"Unexpected course data in {path.name}. Use the verified European retail disc.")
    return data


def signed_normal(packed, shift, bits):
    sign = 1 << (bits - 1)
    value = packed >> shift & (2 * sign - 1)
    return ((value ^ sign) - sign) / (sign - 1)


def read_draw(archive, spec):
    vertex_offset, vertex_count, strip_offset, strip_count, base, material, identity, _ = spec
    vertex_format = struct.Struct("<3fI2f")
    vertices = bytearray()
    for x, y, z, normal, u, v in vertex_format.iter_unpack(
        archive[vertex_offset:vertex_offset + vertex_count * vertex_format.size]
    ):
        nx = signed_normal(normal, 0, 11)
        ny = signed_normal(normal, 11, 11)
        nz = signed_normal(normal, 22, 10)
        vertices.extend(struct.pack("<12f", x, y, z, 1, nx, ny, nz, 1, u, v, 0, 1))
    strip = tuple(i - base for i in struct.unpack_from(f"<{strip_count}H", archive, strip_offset))
    if min(strip) < 0 or max(strip) >= vertex_count:
        raise ValueError("A rail mesh references an unexpected vertex")
    indices = bytearray()
    for i, (a, b, c) in enumerate(zip(strip, strip[1:], strip[2:])):
        indices.extend(struct.pack("<3I", *((b, a, c) if i % 2 else (a, b, c))))
    draw_header = struct.pack("<5I3i", material, 1011209717, len(Draw.slots), vertex_count, len(indices) // 4, *Draw.slots)
    if sha256(draw_header + vertices + indices).hexdigest() != identity:
        raise ValueError(f"The rail draw for material {material} does not match the tested mesh")
    return Draw(
        np.frombuffer(vertices, dtype="<f4").reshape(vertex_count, 3, 4),
        np.frombuffer(indices, dtype="<u4").reshape(-1, 3),
        material, identity,
    )


def build_pack(game):
    archive = read_verified(game / (COURSE + ".img"),
                            "8b6f987dd2423069badd25ca3df6e375479a2313c7a79aa40f30377a2f83babc")
    names = (GROOMED,) + tuple(detail[0] for detail in DETAILS)
    originals = {name: decode_xpr(archive[offset:offset + size])
                 for name, offset, size in directory(archive) if name in names}
    materials = {}
    pack = bytearray(struct.pack("<8sI", b"AMPEDHD2", len(names)))
    for name, artwork, contrast, rotation in ((GROOMED, "groomed-snow", None, 0),) + DETAILS:
        compressed, original = originals[name]
        width, height = original.size
        art = HERE / "art" / (artwork + ".png")
        if contrast is None:
            top, middle = groomed(original, art)
        else:
            detail = tile_detail(art, (width * 4, height * 4), rotation, contrast)
            top, middle = replace_texture(original, detail)
        pack.extend(struct.pack("<4I", width, height, BC1, 2))
        pack.extend(compressed)
        pack.extend(top.tobytes("raw", "BGRA"))
        pack.extend(middle.tobytes("raw", "BGRA"))
        materials[name] = compressed
    if sha256(pack).hexdigest() != PACK_SHA:
        raise ValueError("The generated HD pack did not reproduce the verified output")
    return pack, materials


def c_bytes(name, data):
    rows = (", ".join(f"0x{value:02x}" for value in data[i:i + 16])
            for i in range(0, len(data), 16))
    return "static const unsigned char " + name + "[] = {\n    " + ",\n    ".join(rows) + "\n};\n"


def build_header(game, materials):
    archive = read_verified(game / (COURSE + ".res"),
                            "123bfbbde2fba66e19d295c940864b796e0b3622e70bfdd55c80d726dc66835a")
    parts = ["/* Generated from a verified local disc. */\n"]
    for tag, (name, *_recipe) in zip(("bare", "red", "deck", "trim"), DETAILS[-4:], strict=True):
        parts.append(c_bytes("rail_texture_" + tag, materials[name]))
    for slot, spec in enumerate(MESHES):
        draw = read_draw(archive, spec)
        vertices, indices, _ = spec[-1](draw)
        for field, values in (("original_vertices", draw.vertices), ("original_indices", draw.indices),
                              ("vertices", vertices), ("indices", indices)):
            parts.append(c_bytes(f"mesh_{slot}_{field}", values.tobytes()))
    return "\n".join(parts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    pack, materials = build_pack(args.game)
    header = build_header(args.game, materials)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "amped_visual_data.h").write_text(header, encoding="ascii")
    (args.output / PACK).write_bytes(pack)
    print(f"Verified eight texture overrides and generated three rail draws ({PACK_SHA})")


if __name__ == "__main__":
    main()

"""Derive release visual assets from a verified European retail disc."""

import argparse
from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path
import struct
import sys

import numpy as np

HERE = Path(__file__).resolve().parent
# The embeddable Windows Python uses a fixed module search path.
sys.path[:0] = [str(HERE), str(HERE.parents[2] / "research/vrtdrive")]

from bevel_posts import bevel
from refine_rail import refine
from round_funbox import round_lip
from boughs import foliage_level, needle_detail
from sandstone import CLIFFS, append_cliffs
from safety_nets import NETS, append_nets
from powder import wind_powder_levels
from textures import BC1, BC2, charcoal_levels, decode_xpr, painted_levels, ramp_snow_level, replace_close_texture, replace_texture, tile_detail
from inspect_directory import directory

COURSE = "Resource/Terrain/SS_EgoTrip"
# Keep slots 5–8 stable: the renderer also uses those identities for rail draws.
RAILS = (
    ("mm_rail_metalpole.xpr", "bare-steel", 7.5, 0, 2),
    ("mm_rail_metalpole_red.xpr", "red-enamel", 6.0, 0, 2),
    ("mm_funboxes_silvertop.xpr", "brushed-silver", 6.5, 0, 2),
    ("mm_funboxes_metaledge.xpr", "bare-steel", 5.5, 90, 2),
)
DETAILS = (
    ("ter_snow_groomed_tracks_5x5.xpr", "groomed-snow", 5.5, 0, 2),
    ("ter_snow_packed_4x4.xpr", "packed-snow", 6.0, 0, 2),
    ("ter_snow_powder_rough_3x3.xpr", "wind-worked-snow", 11.0, 0, 2),
    ("pinebark.xpr", "pine-bark", 7.5, 0, 2),
    *RAILS,
    ("b_wood_siding.xpr", "wood-grain", 5.5, 0, 1),
    ("rampside_7x1.xpr", "ramp-scrapes", 8.5, 0, 1),
)
FOLIAGE = ("dpbrns2.xpr", "ppbough.xpr", "ppboughsnow.xpr")
CHARCOAL = "mm_generic_color_charcoal.xpr"
PAINTED_WOOD = "mm_funboxes_red_painted_wood.xpr"
LIFT_METAL = "mm_metal_green.xpr"
MATERIAL_COUNT = len(DETAILS) + len(FOLIAGE) + len(CLIFFS) + len(NETS) + 3
PACK = "snow-summit-visible.ahd"
PACK_SHA = "58390653d6aeb09bcc498ac94e1c3a2ca3f933b16503fbf5cc21a9a1772b63df"

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
    names = {detail[0] for detail in DETAILS} | {CHARCOAL, PAINTED_WOOD, LIFT_METAL}
    originals = {name: decode_xpr(archive[offset:offset + size])
                 for name, offset, size in directory(archive) if name in names}
    materials = {}
    pack = bytearray(struct.pack("<8sI", b"AMPEDHD2", MATERIAL_COUNT))
    for name, artwork, contrast, rotation, added_mips in DETAILS:
        compressed, original = originals[name]
        width, height = original.size
        art = HERE / "art" / (artwork + ".png")
        if artwork == "wind-worked-snow":
            levels = wind_powder_levels(original, art, rotation, contrast)
        else:
            detail = tile_detail(art, (width << added_mips, height << added_mips), rotation, contrast)
            levels = (replace_close_texture(original, detail),) if added_mips == 1 else replace_texture(original, detail)
        if name == "rampside_7x1.xpr":
            # Keep the authored scrapes at midrange. Add fine grain up close.
            levels = (ramp_snow_level(levels[0], HERE / "art/wind-worked-snow.png"), *levels)
        pack.extend(struct.pack("<4I", width, height, BC1, len(levels)))
        pack.extend(compressed)
        for level in levels:
            pack.extend(level.tobytes("raw", "BGRA"))
        materials[name] = compressed
    trees = read_verified(game / COURSE / "Trees.img",
                          "5840045ef1ee840af1160be1afd09f383ee03c45afdf8e04d24eacd89c7a4929")
    branches = {name: decode_xpr(trees[offset:offset + size], color_format=BC2)
                for name, offset, size in directory(trees) if name in FOLIAGE}
    needles = needle_detail(HERE / "art/pine-needles.png")
    for name in FOLIAGE:
        compressed, original = branches[name]
        close = foliage_level(original, None if name == "dpbrns2.xpr" else needles)
        pack.extend(struct.pack("<4I", *original.size, BC2, 1))
        pack.extend(compressed)
        pack.extend(close.tobytes("raw", "BGRA"))
    canyon = read_verified(game / "Resource/Terrain/Fantasy_Southwest.img",
                          "866abd2fa33bee7fa04b9ba30f9ce758b0714072b4d25d4da818e94fa88c0d91")
    append_cliffs(pack, canyon, HERE / "art/sandstone-grain.png")
    compressed, original = originals[CHARCOAL]
    levels = charcoal_levels(original)
    pack.extend(struct.pack("<4I", *original.size, BC1, len(levels)))
    pack.extend(compressed)
    for level in levels:
        pack.extend(level.tobytes("raw", "BGRA"))
    append_nets(pack, game / "Resource/Terrain")
    compressed, original = originals[PAINTED_WOOD]
    levels = painted_levels(original, HERE / "art/wood-grain.png")
    pack.extend(struct.pack("<4I", *original.size, BC1, len(levels)))
    pack.extend(compressed)
    for level in levels:
        pack.extend(level.tobytes("raw", "BGRA"))
    compressed, original = originals[LIFT_METAL]
    width, height = original.size
    detail = tile_detail(HERE / "art/red-enamel.png", (4 * width, 4 * height), 0, 4.5)
    levels = replace_texture(original, detail)
    pack.extend(struct.pack("<4I", width, height, BC1, len(levels)))
    pack.extend(compressed)
    for level in levels:
        pack.extend(level.tobytes("raw", "BGRA"))
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
    for tag, (name, *_recipe) in zip(("bare", "red", "deck", "trim"), RAILS, strict=True):
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
    print(f"Verified {MATERIAL_COUNT} texture overrides and generated {len(MESHES)} rail draws ({PACK_SHA})")


if __name__ == "__main__":
    main()

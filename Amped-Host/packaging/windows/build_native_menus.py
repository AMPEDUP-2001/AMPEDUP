"""Create local overrides for the native Quit Game and Graphics entries."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
HOST = ROOT.parents[1]
sys.path[:0] = [str(HOST / "packaging/windows"), str(HOST / "research/vrtdrive")]
from gbf_known_prefix import known_prefix
from make_menu_entries import directory, directory_edits, menu_edits, patched

END = b"\x01\x00\x00\xff"
SECTION = b"\x00\x00\x00\xff"
QUIT_LABELS = {"MainMenuButton6": "Quit Game", "MainMenuButton6Help": "Close AMPEDUP!"}
GRAPHICS_LABELS = (("OptionsMenuButton5", "OptionsMenuButton6", "Graphics"),
                   ("OptionsMenuButton5Help", "OptionsMenuButton6Help", "Change graphics settings"))


def u32(data, position):
    return struct.unpack_from("<I", data, position)[0]


def lookup(nodes, name):
    node, = [item for item in nodes if item["name"] == name]
    return node


def renamed(data, donor, extra):
    return data.replace(f"button{donor}".encode(), f"button{extra}".encode()).replace(
        f"butt{donor}color".encode(), f"butt{extra}color".encode()
    )


def clone_graphics_button(model):
    donor, extra = 5, 6
    nodes, frontier = known_prefix(model, max_nodes=256)
    if not frontier or frontier["class"] != 0x200000 or model[-4:] != END:
        raise ValueError("Unknown menu scene layout")
    parent = lookup(nodes, "extra_panel_stuff_extern_user")
    group = lookup(nodes, f"button{donor}_group")
    start = group["offset"]
    end = min(n["offset"] for n in nodes if n.get("offset", 0) > start and n["depth"] <= group["depth"])
    clone = bytearray(renamed(model[start:end], donor, extra))
    old_text, new_text, _ = GRAPHICS_LABELS[0]
    if clone.count(old_text.encode()) != 1 or len(old_text) != len(new_text):
        raise ValueError("Unexpected text node identifier")
    clone = clone.replace(old_text.encode(), new_text.encode())
    # Match the extra panel animation path from MainMenu and cancel the
    # Options panel's parent translation.
    values = struct.unpack_from("<7f", model, parent["offset"] + 36)
    if values[3:] != (0.0, 0.0, 0.0, 1.0):
        raise ValueError("Unexpected extra panel orientation")
    struct.pack_into("<3f", clone, 36, *(-value for value in values[:3]))
    # Leave a small clearance for the ice rim while keeping the row near
    # the plane of the other buttons.
    depth = struct.unpack_from("<f", clone, 44)[0]
    struct.pack_into("<f", clone, 44, depth - 0.20)
    for part in ("trans", "rim_trans"):
        node = lookup(nodes, f"button{donor}_{part}")
        offset = node["offset"] - start + 40
        y = struct.unpack_from("<f", clone, offset)[0]
        struct.pack_into("<f", clone, offset, y - 0.67)

    # Each mesh in these two menu files has one 16-byte part following its
    # vertex and index buffers. Validate every record boundary before copying.
    records = {}
    positions = sorted((model.index(n["name"].encode() + b"\0", frontier["offset"]), n["name"])
                       for n in nodes if n["class"] == 0x20000)
    geom_start = positions[0][0]
    if model[geom_start - 12:geom_start - 4] != END + SECTION or u32(model, geom_start - 4) != len(positions):
        raise ValueError("Unknown mesh section layout")
    for i, (pos, name) in enumerate(positions):
        vertex_end = pos + 68 + u32(model, pos + 36)
        if vertex_end + 4 >= len(model):
            raise ValueError("Vertex data extends beyond the model")
        mesh_end = vertex_end + 4 + u32(model, vertex_end) + 16
        following_offset = positions[i + 1][0] if i + 1 < len(positions) else len(model) - 4
        if mesh_end != following_offset:
            raise ValueError(f"Unexpected mesh length for {name}")
        records[name] = model[pos:mesh_end]

    # Unique plate and lettering materials allow the new selection to animate
    # independently of the donor. The rim keeps its existing material.
    mat_start = model.index(b"panel_ice\0", frontier["offset"])
    mat_end = geom_start - 12
    mat_count = u32(model, mat_start - 4)
    if model[mat_start - 8:mat_start - 4] != SECTION or mat_count != 14:
        raise ValueError("Unknown material section layout")
    materials = []
    for name in (f"butt{donor}color", f"inner_color_X_button{donor}"):
        offset = model.index(name.encode() + b"\0", mat_start, mat_end)
        record = model[offset:offset + 108]
        if len(record) != 108 or offset + len(record) > mat_end:
            raise ValueError("Unexpected material record size")
        materials.append(renamed(record, donor, extra))
    meshes = []
    for i, kind in enumerate(("meshShape", "text_meshShape", "rim_frontShape", "rim_front1Shape")):
        entry = bytearray(renamed(records[f"button{donor}_{kind}"], donor, extra))
        if i < 2:
            expected = 10 + i
            material_slot = len(entry) - 16
            if u32(entry, material_slot) != expected:
                raise ValueError("Unexpected source material binding")
            struct.pack_into("<I", entry, material_slot, mat_count + i)
        if i != 1:
            stride = u32(entry, 40)
            vertex_end = 68 + u32(entry, 36)
            if stride != 24 or (vertex_end - 68) % stride:
                raise ValueError("Unexpected native button vertex layout")
            for pos in range(68, vertex_end, stride):
                x, y = struct.unpack_from("<2f", entry, pos)
                struct.pack_into("<2f", entry, pos, x * 0.96, y * 0.86)
        meshes.append(entry)

    child_offset = parent["offset"] + 64
    if parent["child_count"] != 0:
        raise ValueError("Unexpected button parent")
    result = patched(model, sorted([
        (child_offset, 2, struct.pack("<H", parent["child_count"] + 1)),
        (child_offset + 2, 0, clone),
        (mat_start - 4, 4, struct.pack("<I", mat_count + len(materials))),
        (mat_end, 0, b"".join(materials)),
        (geom_start - 4, 4, struct.pack("<I", len(positions) + len(meshes))),
        (len(model) - 4, 0, b"".join(meshes)),
    ]))
    tree, _ = known_prefix(result, max_nodes=256)
    lookup(tree, f"button{extra}_text")
    for kind in ("meshShape", "text_meshShape", "rim_frontShape", "rim_front1Shape"):
        if result.count(f"button{extra}_{kind}\0".encode()) != 2:
            raise ValueError("New scene node and geometry must both be present")
    return result


def label_record(source, key, text, scale=1.0):
    text_at = source[0] + 4
    metadata = source[text_at + 2 * struct.unpack_from("<H", source, text_at - 2)[0]:]
    if len(metadata) not in (5, 9):
        raise ValueError("Unexpected label style record")
    if scale != 1.0:
        size = struct.unpack_from("<f", metadata, 1)[0]
        metadata = metadata[:1] + struct.pack("<f", size * scale) + metadata[5:]
    return (bytes([len(key)]) + key.encode("ascii") + b"\0" +
            struct.pack("<H", len(text)) + text.encode("utf-16le") + metadata)


def localized_strings(data):
    font_count = u32(data, 0)
    if font_count != 4:
        raise ValueError("Unexpected string table font header")
    header = 5 + 23 * font_count
    count = u32(data, header)
    if not 1000 < count < 3000 or header + 4 * (count + 1) >= len(data):
        raise ValueError("Unexpected string table size")
    end_of_offsets = header + 4 * (count + 1)
    start = end_of_offsets + 4
    if u32(data, end_of_offsets) != len(data) - start:
        raise ValueError("Unexpected string payload size")
    offsets = list(struct.unpack_from(f"<{count}I", data, header + 4))
    if offsets[0] != 0 or offsets != sorted(set(offsets)) or offsets[-1] >= len(data) - start:
        raise ValueError("Unexpected string record order")
    records = {}
    for offset, end in zip(offsets, offsets[1:] + [len(data) - start], strict=True):
        row = data[start + offset:start + end]
        key_len = row[0]
        if key_len + 1 >= len(row) or row[key_len + 1] != 0:
            raise ValueError("Expected a terminated string identifier")
        key = row[1:1 + key_len].decode("ascii")
        records[key] = row
    for key, text in QUIT_LABELS.items():
        records[key] = label_record(records[key], key, text)
    added = {}
    for original, key, text in GRAPHICS_LABELS:
        if key in records:
            raise ValueError("Native label already exists in the source")
        after = original if original.endswith("Help") else original + "Help"
        added.setdefault(after, []).append(label_record(records[original], key, text,
                                                         scale=1.0 if key.endswith("Help") else 0.87))
    offsets = []
    payload = bytearray()
    for key, row in records.items():
        offsets.append(len(payload))
        payload.extend(row)
        for record in added.get(key, []):
            offsets.append(len(payload))
            payload.extend(record)
    if len(offsets) != count + len(GRAPHICS_LABELS):
        raise ValueError("The language table did not retain every native string")
    return (data[:header] + struct.pack(f"<{len(offsets) + 2}I", len(offsets), *offsets, len(payload))
            + payload)


def build_strings(source_path):
    source = source_path.read_bytes()
    if hashlib.sha256(source).hexdigest() != "ecc9eb45e13a943d9157ff4665b82069fc83198f6d72a866f9b2f8dd1ff9f10a":
        raise ValueError("Unsupported source shell archive")
    entries = {name: (start, size) for name, start, size in directory(source)}
    changes = []
    deltas = {}
    for name, (offset, size) in entries.items():
        if name.startswith("stringtable_") and name.endswith(".dat"):
            result = localized_strings(source[offset:offset + size])
            changes.append((offset, size, result))
            deltas[name] = len(result) - size
    if len(changes) != 6:
        raise ValueError("Expected the six original language tables")
    result = patched(source, sorted(changes + directory_edits(source, entries, deltas)))
    verified = {name: size for name, _, size in directory(result)}
    if verified != {name: size + deltas.get(name, 0) for name, (_, size) in entries.items()}:
        raise ValueError("Shell archive failed the directory round trip")
    return result


def graphics_animations(main):
    if u32(main, 12) != 4:
        raise ValueError("Unexpected main menu animation table")
    last_track = main.index(b"button5textbounce") - 8
    tracks = main[16:last_track]
    for name in (b"button6selected", b"button6unselected", b"button6bounce"):
        if tracks.count(name) != 1:
            raise ValueError("Missing source button animation")
    return main[:12] + struct.pack("<I", 3) + tracks


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--game-root", type=Path, required=True)
    cli.add_argument("--destination", type=Path, required=True, help="Installation payload")
    args = cli.parse_args()
    assets = args.game_root / "Resource"
    source = (assets / "Shell/Menus.res").read_bytes()
    recipe = json.loads((HOST / "packaging/windows/hud-patches.json").read_text(encoding="utf-8"))
    if hashlib.sha256(source).hexdigest() != recipe["source_sha256"]:
        raise ValueError("Unsupported source menu archive")
    entries = {name: source[off:off + size] for name, off, size in directory(source)}
    models = {
        "optionsmenu.gbf": clone_graphics_button(entries["optionsmenu.gbf"]),
        "optionsmenu.ani": graphics_animations(entries["mainmenu.ani"]),
    }
    edits = menu_edits(source, replacements=models, native=True)
    output_root = args.destination / "overrides"
    shell = build_strings(assets / "Shell.res")
    for name, mode in recipe["modes"].items():
        data = bytearray(source)
        for offset, old, new in mode["edits"]:
            if data[offset] != old:
                raise ValueError("Existing widescreen patch mismatch")
            if any(start <= offset < start + size for start, size, _ in edits):
                raise ValueError("Widescreen edit overlaps a new native menu resource")
            data[offset] = new
        output = patched(data, edits)
        rebuilt = {name: output[off:off + size] for name, off, size in directory(output)}
        if set(rebuilt) != set(entries) or any(rebuilt[name] != payload for name, payload in models.items()):
            raise ValueError("The new archive directory did not round trip")
        target = output_root / name / "Menus.res"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(output)
        mode["sha256"] = hashlib.sha256(output).hexdigest()
        print(target, mode["sha256"])
    target = output_root / "Shell.res"
    target.write_bytes(shell)
    recipe["native_shell_sha256"] = hashlib.sha256(shell).hexdigest()
    print(target, recipe["native_shell_sha256"])
    recipe["native_quit_replaces_demos"] = True
    recipe.pop("menu_edits", None)
    (args.destination / "hud-patches.json").write_text(json.dumps(recipe, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

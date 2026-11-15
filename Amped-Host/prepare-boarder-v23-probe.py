"""Create opt-in observational copies of two v23 generated chunks."""
from pathlib import Path

root = Path(__file__).resolve().parent
generated = root.parent / "Analysis/seeded-v23/generated"
probes = root / "probes"
probes.mkdir(exist_ok=True)
chunks = {}

def replace_in_function(chunk, name, replacements):
    path = generated / f"recomp_{chunk}.c"
    text = chunks.get(chunk)
    if text is None:
        text = path.read_text(encoding="utf-8")
    start = text.index(f"void {name}(void)\n{{")
    end = text.index("\n}\n", start) + 3
    section = text[start:end]
    for original, replacement in replacements:
        if section.count(original) != 1:
            raise SystemExit(f"{name}: expected one probe anchor: {original!r}")
        section = section.replace(original, replacement, 1)
    out = probes / f"recomp_{chunk}_boarder_v23.c"
    chunks[chunk] = text[:start] + section + text[end:]
    out.write_text(chunks[chunk], encoding="utf-8")
    print(f"Wrote {out}")

hook = "    { extern void amped_trace_boarder_v23(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);\n"
replace_in_function("0004", "sub_0001F240", [
    ("    MEM32(eax + edi * 4) = ebp;\n",
     "    MEM32(eax + edi * 4) = ebp;\n" + hook +
     "      amped_trace_boarder_v23(0, ebp, esi, edi, 0x0001F30Du); }\n"),
])
replace_in_function("0011", "sub_0003C540", [
    ("loc_0003C562: ;\n", "loc_0003C562: ;\n" + hook +
     "      amped_trace_boarder_v23(1, esi, 0, 0, 0x0003C562u); }\n"),
    ("loc_0003C656: ;\n", "loc_0003C656: ;\n" + hook +
     "      amped_trace_boarder_v23(1, esi, 0, 0, 0x0003C656u); }\n"),
])

slot_hook = "    { extern void amped_trace_boarder_slot_v23(uint32_t, uint32_t, uint32_t, uint32_t);\n"
slot_changes = []
for label, stage, result in (("000475F0", 0, "0"), ("00047618", 1, "eax & 0xFFu"),
                             ("0004761C", 2, "0"), ("00047629", 3, "eax")):
    anchor = f"loc_{label}: ;\n"
    slot_changes.append((anchor, anchor + slot_hook +
                         f"      amped_trace_boarder_slot_v23({stage}, ebx, MEM32(esp + 0xC), {result}); }}\n"))
replace_in_function("0014", "sub_000475E0", slot_changes)

node_hook = "    { extern void amped_trace_boarder_node_v23(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);\n"
for function, label, kind in (("sub_00091AD0", "loc_00091AD0", 0),
                               ("sub_00091900", "loc_00091900", 1),
                               ("sub_00091970", "loc_00091970", 2)):
    anchor = label + ": ;\n"
    injected = (anchor + node_hook +
                f"      amped_trace_boarder_node_v23({kind}, ecx, MEM32(esp), MEM32(esp + 4), 0); }}\n")
    changes = [(anchor, injected)]
    if kind == 2:
        changes.append(("loc_000919AA: ;\n", "loc_000919AA: ;\n" + node_hook +
                        "      amped_trace_boarder_node_v23(3, esi, MEM32(esp + 8), edi, eax & 0xFFu); }\n"))
    replace_in_function("0024", function, changes)

# Main scene traverses vtable +0x1C, distinct from the +0x20 shadow-like path.
main_hook = "    { extern void amped_trace_boarder_main_v23(uint32_t, uint32_t, uint32_t, uint32_t);\n"
replace_in_function("0024", "sub_00091A90", [
    ("loc_00091A90: ;\n", "loc_00091A90: ;\n" + main_hook +
     "      amped_trace_boarder_main_v23(0, ecx, MEM32(esp), 0); }\n"),
    ("loc_00091A9F: ;\n", "loc_00091A9F: ;\n" + main_hook +
     "      amped_trace_boarder_main_v23(1, edi, MEM32(esp + 4), eax); }\n"),
])
replace_in_function("0024", "sub_000918C0", [
    ("loc_000918C0: ;\n", "loc_000918C0: ;\n" + main_hook +
     "      amped_trace_boarder_main_v23(2, ecx, MEM32(esp), 0); }\n"),
    ("loc_000918CE: ;\n", "loc_000918CE: ;\n" + main_hook +
     "      amped_trace_boarder_main_v23(3, esi, MEM32(esp + 8), eax); }\n"),
])

leaf_hook = "    { extern void amped_trace_boarder_leaf_v23(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);\n"
replace_in_function("0024", "sub_00091BC0", [
    ("loc_00091BC0: ;\n", "loc_00091BC0: ;\n" + leaf_hook +
     "      amped_trace_boarder_leaf_v23(0, ecx, MEM32(esp), MEM32(esp + 4), 0); }\n"),
    ("loc_00091C4B: ;\n", "loc_00091C4B: ;\n" + leaf_hook +
     "      amped_trace_boarder_leaf_v23(1, esi, MEM32(esp + 8), edi, eax & 0xFFu); }\n"),
])
draw_hook = "    { extern void amped_trace_boarder_draw_v23(uint32_t, uint32_t, uint32_t, uint32_t);\n"
draw_changes = [
    ("loc_000A2910: ;\n", "loc_000A2910: ;\n" + leaf_hook +
     "      amped_trace_boarder_leaf_v23(2, ecx, MEM32(esp), MEM32(esp + 4), 0); }\n" + draw_hook +
     "      amped_trace_boarder_draw_v23(0, ecx, 0x000A2910u, 0); }\n"),
]
for label, phase in (("000A295D", 1), ("000A2AA6", 2)):
    anchor = f"loc_{label}: ;\n"
    draw_changes.append((anchor, anchor + draw_hook +
                         f"      amped_trace_boarder_draw_v23({phase}, esi, 0x{label}u, 0); }}\n"))
for label in ("000A2975", "000A29B9", "000A2A07", "000A2A1A", "000A2A6F", "000A2AF6"):
    anchor = f"loc_{label}: ;\n"
    draw_changes.append((anchor, anchor + draw_hook +
                         f"      if ((int32_t)eax < 0) amped_trace_boarder_draw_v23(5, esi, 0x{label}u, eax); }}\n"))
for ret, phase in (("000A2A55", 3), ("000A2B66", 4)):
    anchor = f"    PUSH32(esp, 0x{ret}u); RECOMP_ABI_CALL(0x000FD590u, sub_000FD590); /* call 0x000FD590 */\n"
    draw_changes.append((anchor, draw_hook +
                         f"      amped_trace_boarder_draw_v23({phase}, esi, 0x{ret}u, esp); }}\n" + anchor))
replace_in_function("0027", "sub_000A2910", draw_changes)

cull_hook = "    { extern void amped_trace_cull_v23(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);\n"
replace_in_function("0026", "sub_0009BF70", [
    ("loc_0009C01A: ;\n", "loc_0009C01A: ;\n" + cull_hook +
     "      amped_trace_cull_v23(0, edi, MEM32(ebp + 0xC), edx, ecx); }\n"),
    ("loc_0009C02C: ;\n", "loc_0009C02C: ;\n" + cull_hook +
     "      amped_trace_cull_v23(1, edi, MEM32(ebp + 0xC), edx, 0); }\n"),
])
replace_in_function("0026", "sub_0009D2B0", [
    ("loc_0009D2C4: ;\n", "loc_0009D2C4: ;\n" + cull_hook +
     "      amped_trace_cull_v23(2, esi, ebx, eax & 0xFFu, 0); }\n"),
])
distance_hook = "    { extern void amped_trace_distance_v23(uint32_t, uint32_t, uint32_t, double, double);\n"
compare_anchor = "    g_fp_cmp = RECOMP_FCMP(fp_top(), fp_st1()); g_fp_cc = RECOMP_FCMP_CC(g_fp_cmp); fp_pop(); fp_pop(); /* fcompp  */\n"
replace_in_function("0026", "sub_0009C840", [
    (compare_anchor, distance_hook +
     "      amped_trace_distance_v23(esi, MEM32(ebp + 0xC), ebp - 0x30, fp_top(), fp_st1()); }\n" + compare_anchor),
])
fallback_hook = "    { extern void amped_trace_fallback_v23(uint32_t, uint32_t, uint32_t, uint32_t);\n"
replace_in_function("0026", "sub_0009D2CE", [
    ("loc_0009D2D7: ;\n", "loc_0009D2D7: ;\n" + cull_hook +
     "      amped_trace_cull_v23(3, esi, ebx, eax & 0xFFu, 0); }\n"),
    ("loc_0009D2F1: ;\n", "loc_0009D2F1: ;\n" + cull_hook +
     "      amped_trace_cull_v23(4, esi, ebx, MEM8(esp + 0x14), 0); }\n" + fallback_hook +
     "      amped_trace_fallback_v23(1, esi, eax, MEM8(esp + 0x14)); }\n"),
    ("loc_0009D2E3: ;\n", "loc_0009D2E3: ;\n" + fallback_hook +
     "      amped_trace_fallback_v23(0, esi, 0, 0); }\n"),
])

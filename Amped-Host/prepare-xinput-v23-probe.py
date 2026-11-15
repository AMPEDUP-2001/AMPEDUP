"""Generate a v23-only GetState tracer without modifying generated sources."""
from pathlib import Path

root = Path(__file__).resolve().parent
source = root.parent / "Analysis/seeded-v23/generated/recomp_0072.c"
output = root / "probes/recomp_0072_xinput_v23.c"
text = source.read_text(encoding="utf-8")
start = "void sub_0016AAE9(void)\n{\n"
end = "loc_0016AB53: ;\n    eax = esi;\n"
if text.count(start) != 1 or text.count(end) != 1:
    raise SystemExit("v23 GetState probe anchors changed; inspect generated code")
entry = (start + "    uint32_t amped_handle = MEM32(esp + 4), amped_out = MEM32(esp + 8);\n"
         "    extern void amped_trace_getstate_v23(uint32_t, uint32_t, uint32_t);\n")
traced_end = end + "    amped_trace_getstate_v23(amped_handle, amped_out, eax);\n"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(text.replace(start, entry).replace(end, traced_end), encoding="utf-8")
print(f"Wrote {output}")

source38 = root.parent / "Analysis/seeded-v23/generated/recomp_0038.c"
output38 = root / "probes/recomp_0038_input_v23.c"
text38 = source38.read_text(encoding="utf-8")
anchor = "    MEMF(esi + 0xA0) = (float)fp_top(); fp_pop(); /* fstp */\n    MEM32(esi + 0xA8) = eax;\n    MEM32(esi + 0xB0) = ebp;\n"
start38 = text38.index("void sub_000C60D0(void)\n{")
end38 = text38.index("\n}\n", start38) + 3
section38 = text38[start38:end38]
if section38.count(anchor) != 1:
    raise SystemExit("v23 input edge probe anchor changed; inspect generated code")
traced = (anchor + "    { extern void amped_trace_input_edge_v23(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);\n"
          "      amped_trace_input_edge_v23(esi, MEM8(esp + 0x22), MEM8(esi + 0x80), MEM32(esi + 0xA4), MEM32(esi + 0xAC), MEM32(esi + 0xA8)); }\n")
output38.write_text(text38[:start38] + section38.replace(anchor, traced) + text38[end38:], encoding="utf-8")
print(f"Wrote {output38}")

# Instrument two guest button-edge getters in isolated chunks only.
def instrument_getter(chunk, fn, ret, expected, kind, requested):
    path = root.parent / f"Analysis/seeded-v23/generated/recomp_{chunk}.c"
    out = root / f"probes/recomp_{chunk}_button_v23.c"
    text = path.read_text(encoding="utf-8")
    start = text.index(f"void {fn}(void)\n{{")
    end = text.index("\n}\n", start) + 3
    section = text[start:end]
    entry = f"void {fn}(void)\n{{\n"
    pre = (entry + f"    uint32_t amped_mask = MEM32(ecx), amped_requested = {requested}, amped_ret = MEM32(esp), amped_selected = MEM32(0x19E9A0), amped_mode = MEM32(0x19EF20);\n"
           "    extern void amped_trace_button_v23(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);\n")
    hook = (f"    amped_trace_button_v23({kind}, amped_mask, amped_requested, amped_ret, eax & 0xFFu, amped_selected, amped_mode);\n" + ret)
    if section.count(entry) != 1 or section.count(ret) != expected:
        raise SystemExit(f"v23 button getter {fn} anchors changed; inspect generated code")
    section = section.replace(entry, pre, 1).replace(ret, hook)
    out.write_text(text[:start] + section + text[end:], encoding="utf-8")
    print(f"Wrote {out}")

instrument_getter("0003", "sub_0001BF70", "    esp += 8; return; /* ret 4 */\n", 1, 1, "MEM32(esp + 4)")
instrument_getter("0037", "sub_000C5AC0", "    esp += 4; return; /* ret */\n", 3, 2, "0xFFFFFFFFu")

# Observe the camera-mode cycle without editing the original generated chunk.
source09 = root.parent / "Analysis/seeded-v23/generated/recomp_0009.c"
output09 = root / "probes/recomp_0009_camera_v23.c"
text09 = source09.read_text(encoding="utf-8")
start09 = text09.index("void sub_00033E50(void)\n{")
end09 = text09.index("\n}\n", start09) + 3
section09 = text09[start09:end09]
entry09 = "void sub_00033E50(void)\n{\n"
return09 = "    esp += 12; return; /* ret 8 */\n"
if section09.count(entry09) != 1 or section09.count(return09) != 1:
    raise SystemExit("v23 camera probe anchors changed; inspect generated code")
hook09 = (entry09 +
          "    uint32_t amped_camera = ecx, amped_caller = MEM32(esp);\n"
          "    uint32_t amped_camera_arg = MEM8(esp + 4), amped_camera_slot = MEM32(esp + 8);\n"
          "    extern void amped_trace_camera_v23(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);\n"
          "    amped_trace_camera_v23(0, amped_camera, amped_caller, amped_camera_arg, amped_camera_slot);\n")
exit09 = ("    amped_trace_camera_v23(1, amped_camera, amped_caller, amped_camera_arg, amped_camera_slot);\n" + return09)
section09 = section09.replace(entry09, hook09, 1).replace(return09, exit09, 1)
output09.write_text(text09[:start09] + section09 + text09[end09:], encoding="utf-8")
print(f"Wrote {output09}")

"""Prepare Amped's matched v23 submission, fence and IRQ hooks from the original generated code."""
from pathlib import Path

root = Path(__file__).resolve().parent
source = root.parent / "Analysis/seeded-v23/generated/recomp_0058.c"
target = root / "probes/recomp_0058_kick_v23.c"
text = source.read_text(encoding="utf-8")
start = text.index("void sub_000FE8E0(void)\n{")
end = text.index("\n}\n", start) + 3
body = text[start:end]
before = "    if (CMP_NE(_fa, _fb)) goto loc_000FE920; /* jne: not equal / not zero */"
after = ("    if (CMP_NE(_fa, _fb)) {\n"
         "        if (amped_kick_wait) xbox_SpinWait(0x000FE920u);\n"
         "        goto loc_000FE920;\n"
         "    } /* optional KickOff backedge wait */")
if body.count(before) != 1:
    raise SystemExit("KickOff backedge anchor did not match exactly once")
locals_anchor = "    int _flags = 0; /* fallback flag var */\n"
if body.count(locals_anchor) != 1:
    raise SystemExit("KickOff locals anchor did not match exactly once")
body = body.replace(locals_anchor, locals_anchor + "    int amped_kick_wait = amped_kick_wait_enabled_v23();\n"
                    "    uint64_t amped_kick_profile_seq = 0, amped_kick_profile_start = 0;\n", 1)
write_anchor = "    MEM32(eax + 0x100410) = edx;\n"
exit_anchor = "\nloc_000FE92C: ;\n"
if body.count(write_anchor) != 1 or body.count(exit_anchor) != 1:
    raise SystemExit("KickOff profiling anchors missing or ambiguous")
body = body.replace(write_anchor,
    "    amped_kick_profile_start = xbox_KickProfBegin(&amped_kick_profile_seq);\n" + write_anchor, 1)
body = body.replace(exit_anchor,
    "\n    xbox_KickProfEnd(amped_kick_profile_start, amped_kick_profile_seq);\n" + exit_anchor, 1)
text = text[:start] + body.replace(before, after, 1) + text[end:]
fence_entry = "void sub_000FEA30(void)\n{\n"
fence_branch = "    if (CMP_B(_fa, _fb)) goto loc_000FEAE4; /* jb: below (unsigned <) */"
if text.count(fence_entry) != 1 or text.count(fence_branch) != 1:
    raise SystemExit("Expected the Amped fence polling backedge exactly once")
text = text.replace(fence_entry, fence_entry + "    const int wait_fence = xbox_AmpedFenceWait();\n", 1)
text = text.replace(fence_branch, "    if (CMP_B(_fa, _fb)) {\n"
                    "        if (wait_fence) xbox_SpinWait(0x000FEAE4u);\n"
                    "        goto loc_000FEAE4;\n    }", 1)
header = '#include "recomp_funcs.h"\n'
if text.count(header) != 1:
    raise SystemExit("generated include anchor did not match exactly once")
text = text.replace(header, header + '#include "kick_profile.h"\n' +
                    "extern int amped_kick_wait_enabled_v23(void);\nextern int xbox_AmpedFenceWait(void);\n", 1)
target.write_text(text, encoding="utf-8")
print(f"Wrote {target}")

# The title acknowledges the vertical blank with a write-one-to-clear MMIO
# store, then reads the master interrupt register until it sees the change.
source = source.with_name("recomp_0059.c")
target = target.with_name("recomp_0059_ack_v23.c")
text = source.read_text(encoding="utf-8")
ack = "loc_00103D30: ;\n    MEM32(edi + 0x600100) = ebx;\n"
entry = "void sub_00103C50(void)\n{\n"
if text.count(ack) != 1 or text.count(entry) != 1 or text.count(header) != 1:
    raise SystemExit("Expected v23 PCRTC acknowledge instruction exactly once")
text = text.replace(header, header + "extern int xbox_AmpedDirectAckEnabled(void);\n"
                    "extern int xbox_AmpedVblankWrite(uint32_t, uint32_t);\n", 1)
text = text.replace(entry, entry + "    const int direct_ack = xbox_AmpedDirectAckEnabled();\n", 1)
text = text.replace(ack, "loc_00103D30: ;\n"
                    "    if (!direct_ack || !xbox_AmpedVblankWrite(edi, ebx)) MEM32(edi + 0x600100) = ebx;\n", 1)
target.write_text(text, encoding="utf-8")
print(f"Wrote {target}")

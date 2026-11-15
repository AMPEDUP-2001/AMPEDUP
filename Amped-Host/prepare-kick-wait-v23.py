"""Create a v23-only KickOff backedge experiment without editing generated code."""
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
header = '#include "recomp_funcs.h"\n'
if text.count(header) != 1:
    raise SystemExit("generated include anchor did not match exactly once")
text = text.replace(header, header + '#include "kick_profile.h"\n' +
                    "extern int amped_kick_wait_enabled_v23(void);\n", 1)
target.write_text(text, encoding="utf-8")
print(f"Wrote {target}")

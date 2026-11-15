from pathlib import Path
p = Path(__file__).resolve()
root = p.parents[3]
s = (root / 'Analysis/seeded-v23/generated/recomp_0007.c').read_text()
a = s.index('void sub_0002BC20(void)\n{')
b = s.index('\n}\n', a) + 3
body = s[a:b]
old53 = 'if (CMP_NE(_fa, _fb)) { g_seh_ebp = ebp; sub_0002BC53(); return; }'
old5d = 'if (CMP_NE(_fa, _fb)) { g_seh_ebp = ebp; sub_0002BC5D(); return; }'
assert body.count(old53) == body.count(old5d) == 1
body = body.replace(old53, 'if (CMP_NE(_fa, _fb)) goto loc_audio_2BC53;').replace(old5d, 'if (CMP_NE(_fa, _fb)) goto loc_audio_2BC5D;')
anchor = '    g_seh_ebp = ebp; sub_0002BC47(); return; /* fallthrough 0x0002BC47 */\n'
block = '''
/* Original in-function branch blocks, omitted from v23 function discovery. */
loc_audio_2BC53: ;
    PUSH32(esp, 1);
    PUSH32(esp, 0x0002BC5Au); RECOMP_ABI_CALL(0x000C8FE0u, sub_000C8FE0);
    esp += 4;
loc_audio_2BC5D: ;
    eax = 0;
    POP32(esp, ecx);
    esp += 4; return;
'''
assert body.count(anchor) == 1
body = body.replace(anchor, anchor + block)
out = s[:a] + body + s[b:]
assert out.replace(block, '').replace('if (CMP_NE(_fa, _fb)) goto loc_audio_2BC53;', old53).replace('if (CMP_NE(_fa, _fb)) goto loc_audio_2BC5D;', old5d) == s
p.with_name('recomp_0007_audio_fix.c').write_text(out)
print('Audio branch reconstruction anchors unique and reversible')

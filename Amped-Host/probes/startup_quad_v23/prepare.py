"""Exact-anchor private E7510 startup experiment. No baseline chunks overwritten."""
import argparse
from pathlib import Path
ANCHORS = {
 '#include "recomp_funcs.h"': '#include "recomp_funcs.h"\nextern void amp_sq_scope(unsigned,unsigned);\nextern void amp_sq_end(void);\nextern void amp_sq_clear_begin(void);\nextern void amp_sq_clear_done(void);\nextern void amp_sq_apply(unsigned);',
 'loc_000E66F2: ;\n    ecx = esi;': 'loc_000E66F2: ;\n    amp_sq_scope(esi,esp);\n    ecx = esi;',
 '    PUSH32(esp, 0x000E66F9u); RECOMP_ABI_CALL(0x000E59A0u, sub_000E59A0); /* call 0x000E59A0 */': '    __try {\n        PUSH32(esp, 0x000E66F9u); RECOMP_ABI_CALL(0x000E59A0u, sub_000E59A0); /* call 0x000E59A0 */\n    } __finally { amp_sq_end(); }',
 'loc_000E59D1: ;\n    ecx = MEM32(esi + 0x30);': 'loc_000E59D1: ;\n    amp_sq_clear_begin();\n    ecx = MEM32(esi + 0x30);',
 '    PUSH32(esp, 0x000E73C7u); RECOMP_ABI_CALL(0x00103160u, sub_00103160); /* call 0x00103160 */': '    PUSH32(esp, 0x000E73C7u); RECOMP_ABI_CALL(0x00103160u, sub_00103160); /* call 0x00103160 */\n    amp_sq_clear_done();',
 '    PUSH32(esp, 0x000E75F6u); RECOMP_ABI_CALL(0x000FD3B0u, sub_000FD3B0); /* call 0x000FD3B0 */': '    amp_sq_apply(esp + 24u);\n    PUSH32(esp, 0x000E75F6u); RECOMP_ABI_CALL(0x000FD3B0u, sub_000FD3B0); /* call 0x000FD3B0 */',
}
def instrument(s):
 for old,new in ANCHORS.items():
  if s.count(old)!=1: raise ValueError('missing or duplicated anchor: '+old[:60])
  s=s.replace(old,new,1)
 return s
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(instrument(a.source.read_text(encoding='utf8').replace('\r\n','\n')),encoding='utf8',newline='\n')

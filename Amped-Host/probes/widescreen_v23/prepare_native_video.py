"""Read-only guest video-flag observations in a private v23 chunk."""
import argparse
from pathlib import Path
DECL='#include "recomp_funcs.h"'
EXT='\nextern void amped_native_video_flags_v23(unsigned);\nextern void amped_native_video_aspect_v23(unsigned);'
FLAGS='loc_0008F8F3: ;\n    _fa = (uint32_t)((LO8(eax)) & (1))'
FLAGS_HOOK='loc_0008F8F3: ;\n    amped_native_video_flags_v23(eax);\n    _fa = (uint32_t)((LO8(eax)) & (1))'
ASPECT='loc_0008F908: ;\n    PUSH32(esp, 0x8000);'
ASPECT_HOOK='loc_0008F908: ;\n    amped_native_video_aspect_v23(MEM32(0x186F74));\n    PUSH32(esp, 0x8000);'
def instrument(s):
    start=s.index('void sub_0008F840(void)');end=s.index('\nvoid sub_',start+1)
    if s.count(DECL)!=1 or s.count(FLAGS)!=1 or s.count(ASPECT)!=1 or not start<=s.index(FLAGS)<s.index(ASPECT)<end:
        raise ValueError('native video anchors missing, duplicated or outside 8F840')
    return s.replace(DECL,DECL+EXT,1).replace(FLAGS,FLAGS_HOOK,1).replace(ASPECT,ASPECT_HOOK,1)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    s=a.source.read_text(encoding='utf8').replace('\r\n','\n');out=instrument(s);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(out,encoding='utf8',newline='\n')

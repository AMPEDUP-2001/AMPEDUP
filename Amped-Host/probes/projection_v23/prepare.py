"""Create an isolated v23 projection entry observer, preserving lifted code."""
import argparse,hashlib
from pathlib import Path
DECL='#include "recomp_funcs.h"'
ANCHOR='loc_000F7CFA: ;\n    PUSH32(esp, ebp);'
HOOK='    amped_projection_v23(esp);\n'
EXT='\nextern void amped_projection_v23(unsigned);'
def instrument(s, observe=True, ultrawide=False):
    start=s.index('void sub_000F7CFA(void)')
    end=s.index('\nvoid sub_000F7D',start)
    if s.count(DECL)!=1 or s.count(ANCHOR)!=1 or not start<=s.index(ANCHOR)<end:
        raise ValueError('projection anchor missing, duplicated or outside function')
    extra = EXT if observe else ''
    hook = HOOK if observe else ''
    if ultrawide:
        extra += '\nextern void amped_ultrawide_trial_projection(unsigned);'
        hook += '    amped_ultrawide_trial_projection(esp);\n'
    return s.replace(DECL,DECL+extra,1).replace(ANCHOR,'loc_000F7CFA: ;\n'+hook+'    PUSH32(esp, ebp);',1)
def main():
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--no-observer', action='store_true')
    p.add_argument('--ultrawide', action='store_true')
    a=p.parse_args()
    raw=a.source.read_bytes();out=instrument(raw.decode('utf8').replace('\r\n','\n'), not a.no_observer, a.ultrawide)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(out,encoding='utf8',newline='\n')
    print('projection probe source sha256='+hashlib.sha256(raw).hexdigest())
if __name__=='__main__':main()

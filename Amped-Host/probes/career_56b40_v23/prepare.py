from pathlib import Path
import re
p=Path(__file__).resolve(); root=p.parents[3]; t=p.with_name('translated.txt').read_text(encoding='utf-8-sig')
assert t.startswith('/**\n * sub_00056B40') and 'Original: 0x00056B40 - 0x00056C32' in t
assert t.count('void sub_00056B40(void)')==1 and t.count('esp += 8; return; /* ret 4 */')==1
calls=set(re.findall(r'RECOMP_ABI_CALL\(0x[0-9A-F]+u,\s*sub_([0-9A-F]{8})',t))
stub=(root/'Analysis/seeded-v23/generated/recomp_stubs_unresolved.c').read_text(); unresolved=set(re.findall(r'void sub_([0-9A-F]{8})\(',stub))
h=(root/'Analysis/seeded-v23/generated/recomp_funcs.h').read_text(); assert not calls&unresolved, calls&unresolved
missing=[x for x in calls if f'sub_{x}(' not in h]; assert not missing, missing
s=(root/'Amped-Host/probes/multiplayer_callback_v23/recomp_dispatch_multiplayer.c').read_text(); assert '0x00056B40u' not in s
needle='extern void sub_0002BE70(void);'; anchor='    { 0x00056C40u, (recomp_func_t)sub_00056C40 },'; assert s.count(needle)==s.count(anchor)==1
wrapper='''extern void sub_00056B40(void);
static volatile LONG amped_career_56b40_logged;
static void amped_career_56b40_entry(void) {
 DWORD saved_error=GetLastError(); int saved_errno=errno;
 const char *trace=getenv("AMPED_CAREER_56B40_TRACE");
 int marked=trace && strcmp(trace,"1")==0 && InterlockedCompareExchange(&amped_career_56b40_logged,1,0)==0;
 if(marked) (fprintf)(stderr,"[AMPED_CAREER_56B40] guest entry 00056B40\\n");
 errno=saved_errno; SetLastError(saved_error); sub_00056B40();
 if(marked) {
  saved_error=GetLastError(); saved_errno=errno;
  (fprintf)(stderr,"[AMPED_CAREER_56B40] guest return 00056B40\\n");
  errno=saved_errno; SetLastError(saved_error);
 }
}
'''
s=s.replace(needle,wrapper+needle,1).replace(anchor,'    { 0x00056B40u, (recomp_func_t)amped_career_56b40_entry },\n'+anchor,1)
vas=[int(x,16) for x in re.findall(r'\{\s*0x([0-9A-Fa-f]+)u,\s*\(recomp_func_t\)',s)]; assert vas==sorted(vas) and len(vas)==len(set(vas))
p.with_name('career_56b40.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n#include <math.h>\n\n'+t,encoding='utf-8')
p.with_name('recomp_dispatch_career.c').write_text(s,encoding='utf-8')
print('Prepared isolated body and dispatch; direct dependencies declared and not known unresolved; no runtime validation')

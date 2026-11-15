from pathlib import Path
import re
p=Path(__file__).resolve(); root=p.parents[3]; t=p.with_name('translated.txt').read_text(encoding='utf-8-sig')
assert t.startswith('/**\n * sub_0004F880') and 'Original: 0x0004F880 - 0x0004FA54' in t
assert t.count('void sub_0004F880(void)')==1 and t.count('esp += 4; return; /* ret */')==1
calls=set(re.findall(r'RECOMP_ABI_CALL\(0x[0-9A-F]+u,\s*sub_([0-9A-F]{8})',t)); assert calls=={'0001650B','0004F470','000A49E0','000A6320','000F4810'}
stub=(root/'Analysis/seeded-v23/generated/recomp_stubs_unresolved.c').read_text(); unresolved=set(re.findall(r'void sub_([0-9A-F]{8})\(',stub))
h=(root/'Analysis/seeded-v23/generated/recomp_funcs.h').read_text(); assert not calls&unresolved; assert all(f'sub_{x}(' in h for x in calls)
s=(root/'Amped-Host/probes/career_56b40_v23/recomp_dispatch_career.c').read_text(); assert '0x0004F880u' not in s
needle='extern void sub_0002BE70(void);'; anchor='    { 0x0004FA60u, (recomp_func_t)sub_0004FA60 },'; assert s.count(needle)==s.count(anchor)==1
wrapper='''extern void sub_0004F880(void);
static volatile LONG amped_replay_save_logged;
static void amped_replay_save_entry(void) {
 DWORD saved_error=GetLastError(); int saved_errno=errno;
 const char *trace=getenv("AMPED_REPLAY_SAVE_4F880_TRACE");
 int marked=trace && strcmp(trace,"1")==0 && InterlockedCompareExchange(&amped_replay_save_logged,1,0)==0;
 if(marked) (fprintf)(stderr,"[AMPED_REPLAY_SAVE_4F880] guest entry 0004F880\\n");
 errno=saved_errno; SetLastError(saved_error); sub_0004F880();
 if(marked) { saved_error=GetLastError(); saved_errno=errno;
  (fprintf)(stderr,"[AMPED_REPLAY_SAVE_4F880] guest return 0004F880\\n");
  errno=saved_errno; SetLastError(saved_error); }
}
'''
s=s.replace(needle,wrapper+needle,1).replace(anchor,'    { 0x0004F880u, (recomp_func_t)amped_replay_save_entry },\n'+anchor,1)
vas=[int(x,16) for x in re.findall(r'\{\s*0x([0-9A-Fa-f]+)u,\s*\(recomp_func_t\)',s)]; assert vas==sorted(vas) and len(vas)==len(set(vas))
p.with_name('replay_save_4f880.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n#include <math.h>\n\n'+t,encoding='utf-8')
p.with_name('recomp_dispatch_replay_save.c').write_text(s,encoding='utf-8')
print('PASS: exact body and sorted unique dispatch; all five direct callees declared and not known unresolved')

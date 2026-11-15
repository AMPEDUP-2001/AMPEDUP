from pathlib import Path
p=Path(__file__).resolve();root=p.parents[3]
t=p.with_name('translated.txt').read_text(encoding='utf-8-sig')
assert t.startswith('/**\n * sub_000681A0') and 'Original: 0x000681A0 - 0x000685D7' in t
assert t.count('void sub_000681A0(void)')==1
stub=(root/'Analysis/seeded-v23/generated/recomp_stubs_unresolved.c').read_text()
import re
called=set(re.findall(r'sub_([0-9A-F]{8})\(',t));unresolved=set(re.findall(r'void sub_([0-9A-F]{8})\(',stub));assert not called&unresolved, called&unresolved
p.with_name('replay_v23.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n#include <math.h>\n\n'+t)
s=(root/'Amped-Host/probes/playeor/recomp_dispatch_playeor.c').read_text()
assert '0x000681A0u' not in s
anchor='    { 0x000685E0u, (recomp_func_t)sub_000685E0 },';assert s.count(anchor)==1
s=s.replace('extern void sub_0002BE70(void);', '''extern void sub_000681A0(void);
static volatile LONG amped_replay_logged;
static void amped_replay_entry(void) {
 DWORD saved_error=GetLastError(); int saved_errno=errno;
 const char *trace=getenv("AMPED_PLAYEOR_TRACE");
 if(trace && strcmp(trace,"1")==0 && InterlockedCompareExchange(&amped_replay_logged,1,0)==0)
  (fprintf)(stderr,"[AMPED_REPLAY] guest callback 000681A0 entered\\n");
 errno=saved_errno; SetLastError(saved_error); sub_000681A0();
}
extern void sub_0002BE70(void);''',1)
s=s.replace(anchor,'    { 0x000681A0u, (recomp_func_t)amped_replay_entry },\n'+anchor,1)
p.with_name('recomp_dispatch_replay.c').write_text(s)
vas=[int(x,16) for x in re.findall(r'\{\s*0x([0-9A-Fa-f]+)u,\s*\(recomp_func_t\)',s)]
assert vas==sorted(vas) and len(vas)==len(set(vas))
print('Replay body and ordered combined dispatch prepared; no direct unresolved stubs')

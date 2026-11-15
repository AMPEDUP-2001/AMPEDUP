from pathlib import Path
import re
p=Path(__file__).resolve(); root=p.parents[3]
t=p.with_name('translated.txt').read_text(encoding='utf-8-sig')
assert t.startswith('/**\n * sub_00068960') and 'Original: 0x00068960 - 0x00068CA1' in t
assert t.count('void sub_00068960(void)')==1
stub=(root/'Analysis/seeded-v23/generated/recomp_stubs_unresolved.c').read_text()
called=set(re.findall(r'sub_([0-9A-F]{8})\(',t)); unresolved=set(re.findall(r'void sub_([0-9A-F]{8})\(',stub)); assert not called & unresolved, called & unresolved
h=(root/'Analysis/seeded-v23/generated/recomp_funcs.h').read_text(); missing=[x for x in called if x!='00068960' and f'sub_{x}(' not in h]; assert not missing, missing
p.with_name('multiplayer_v23.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n#include <math.h>\n\n'+t,encoding='utf-8')
s=(root/'Amped-Host/probes/replay_callback_v23/recomp_dispatch_replay.c').read_text(); assert '0x00068960u' not in s
anchor='    { 0x00068CB0u, (recomp_func_t)sub_00068CB0 },'; assert s.count(anchor)==1
needle='extern void sub_0002BE70(void);'; assert s.count(needle)==1
s=s.replace(needle, '''extern void sub_00068960(void);
static volatile LONG amped_multiplayer_logged;
static void amped_multiplayer_entry(void) {
 DWORD saved_error=GetLastError(); int saved_errno=errno;
 const char *trace=getenv("AMPED_MULTIPLAYER_CALLBACK_TRACE");
 if(trace && strcmp(trace,"1")==0 && InterlockedCompareExchange(&amped_multiplayer_logged,1,0)==0)
  (fprintf)(stderr,"[AMPED_MULTIPLAYER] guest callback 00068960 entered\\n");
 errno=saved_errno; SetLastError(saved_error); sub_00068960();
}
'''+needle,1)
s=s.replace(anchor,'    { 0x00068960u, (recomp_func_t)amped_multiplayer_entry },\n'+anchor,1)
vas=[int(x,16) for x in re.findall(r'\{\s*0x([0-9A-Fa-f]+)u,\s*\(recomp_func_t\)',s)]; assert vas==sorted(vas) and len(vas)==len(set(vas))
p.with_name('recomp_dispatch_multiplayer.c').write_text(s,encoding='utf-8')
print('Prepared multiplayer callback and unique ordered dispatch; no direct unresolved stubs or missing declarations')

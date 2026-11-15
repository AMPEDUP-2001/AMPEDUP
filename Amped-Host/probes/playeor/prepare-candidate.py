from pathlib import Path
import re
p=Path(__file__).resolve(); root=p.parents[3]
translation=(root/'Analysis/seeded-playeor-v23/playeor-translation.txt').read_text(encoding='utf-8-sig')
assert translation.startswith('/**\n * sub_0002BE70')
assert 'Original: 0x0002BE70 - 0x0002C0E4' in translation
assert translation.count('void sub_0002BE70(void)')==1
assert 'sub_0002C09F()' not in translation
assert 'loc_0002C0B9:' in translation and 'loc_0002C0DA:' in translation
p.with_name('playeor_v23.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n#include <math.h>\n\n'+translation,encoding='utf-8')
toggle=(root/'Analysis/seeded-playeor-pair-v23/toggle-translation.txt').read_text(encoding='utf-8-sig')
assert toggle.startswith('/**\n * sub_0002C0F0')
assert 'Original: 0x0002C0F0 - 0x0002C216' in toggle
assert toggle.count('void sub_0002C0F0(void)')==1 and 'loc_0002C20E:' in toggle
p.with_name('toggle_v23.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n#include <math.h>\n\n'+toggle,encoding='utf-8')
s=(root/'Analysis/seeded-v23/generated/recomp_dispatch.c').read_text()
assert '0x0002BE70u' not in s and '0x0002C0F0u' not in s
anchor='    { 0x0002C09Fu, (recomp_func_t)sub_0002C09F },'
assert s.count(anchor)==1
s=s.replace('#include <stdlib.h>', '''#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <windows.h>
extern void sub_0002BE70(void);
extern void sub_0002C0F0(void);
static volatile LONG amped_playeor_logged;
static void amped_playeor_entry(void) {
    DWORD saved_error = GetLastError(); int saved_errno = errno;
    const char *trace = getenv("AMPED_PLAYEOR_TRACE");
    if (trace && strcmp(trace, "1") == 0 && InterlockedCompareExchange(&amped_playeor_logged, 1, 0) == 0)
        (fprintf)(stderr, "[AMPED_PLAYEOR] guest callback 0002BE70 entered\\n");
    errno = saved_errno; SetLastError(saved_error);
    sub_0002BE70();
}
static volatile LONG amped_toggle_logged;
static void amped_toggle_entry(void) {
    DWORD saved_error = GetLastError(); int saved_errno = errno;
    const char *trace = getenv("AMPED_PLAYEOR_TRACE");
    if (trace && strcmp(trace, "1") == 0 && InterlockedCompareExchange(&amped_toggle_logged, 1, 0) == 0)
        (fprintf)(stderr, "[AMPED_TOGGLE] guest callback 0002C0F0 entered\\n");
    errno = saved_errno; SetLastError(saved_error);
    sub_0002C0F0();
}
''',1)
s=s.replace(anchor,'    { 0x0002BE70u, (recomp_func_t)amped_playeor_entry },\n'+anchor,1)
anchor2='    { 0x0002C220u, (recomp_func_t)sub_0002C220 },'
assert s.count(anchor2)==1
s=s.replace(anchor2,'    { 0x0002C0F0u, (recomp_func_t)amped_toggle_entry },\n'+anchor2,1)
p.with_name('recomp_dispatch_playeor.c').write_text(s,encoding='utf-8')
print('generated opt-in PlayEOR body and sorted dispatch overlay; baseline files untouched')

/* Optional KickOff yield. The legacy flags switch process-wide spin-wait pacing
 * to sleep, which can also affect any other lowered wait sites. The pacing
 * flag alone provides a control without enabling the KickOff backedge hook.
 * AMPED_KICK_LOCAL_WAIT opts only the identified flush loop into sleeping. */
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel_pacing.h"

/* Selected once at startup, before any guest thread is started. */
static int s_potato_wait;
void amped_kick_preset_v23(int enabled)
{
    s_potato_wait = enabled;
}

int amped_kick_wait_enabled_v23(void)
{
    static atomic_int state; /* 0 unread, 1 initializing, 2 off, 3 hook, 4 pacing-only, 5 local */
    int value = atomic_load_explicit(&state, memory_order_acquire);
    if (value < 2) {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&state, &expected, 1,
                                                    memory_order_acq_rel, memory_order_acquire)) {
            const char *e = getenv("AMPED_KICK_WAIT_SLEEP");
            const char *p = getenv("AMPED_KICK_PACING_SLEEP");
            const char *l = getenv("AMPED_KICK_LOCAL_WAIT");
            value = e && !strcmp(e, "1") ? 3 : 2;
            if (value == 2 && p && !strcmp(p, "1")) value = 4;
            if (value == 2 && (l ? !strcmp(l, "1") : s_potato_wait)) value = 5;
            if (value == 3 || value == 4) xbox_PacingSetMode(XBOX_PACING_SLEEP);
            if (value == 5) xbox_PacingSetLocalSpinSite(0x000FE920u);
            (fprintf)(stderr, "[AMPED_KICK_WAIT] mode=%s pacing=%s site=000FE920; site counters: RECOMP_TRACE=pacing\n",
                      value == 3 ? "hook" : value == 4 ? "pacing-only" : value == 5 ? "local" : "off",
                      value == 3 || value == 4 ? "sleep" : "unchanged");
            atomic_store_explicit(&state, value, memory_order_release);
        } else {
            do { value = atomic_load_explicit(&state, memory_order_acquire); } while (value < 2);
        }
    }
    return value == 3 || (value == 5 && xbox_PacingLocalSpinSite() == 0x000FE920u);
}

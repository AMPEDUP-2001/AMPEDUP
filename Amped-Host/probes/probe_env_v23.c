#include "probe_env_v23.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name; char *value; int fallback; } ProbeOption;
static ProbeOption s_options[] = {
    {"AMPED_BOARDER_AGGREGATE", NULL, 0}, {"AMPED_BOARDER_AGG_FRAMES", NULL, 0},
    {"AMPED_BOARDER_TRACE", NULL, 0}, {"AMPED_BOARDER_NODE_FROM", NULL, 0},
    {"AMPED_SCENE_SNAPSHOT", NULL, 0}, {"AMPED_CULL_TRACE", NULL, 0},
    {"AMPED_GETSTATE_TRACE", NULL, 0}, {"AMPED_CAMERA_TRACE", NULL, 0},
    {"AMPED_A_GAME_TRACE", NULL, 0}, {"AMPED_A_GAME_ARM_FILE", NULL, 0},
    {"AMPED_A_ACTION_TRACE", NULL, 0}
};
static atomic_int s_state;
static int s_cache;

const char *amped_probe_getenv(const char *name)
{
    int state = atomic_load_explicit(&s_state, memory_order_acquire);
    size_t i;
    if (state != 2) {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&s_state, &expected, 1,
                                                    memory_order_acq_rel, memory_order_acquire)) {
            const char *e = getenv("AMPED_CACHE_GUEST_PROBES");
            s_cache = e && !strcmp(e, "1");
            if (s_cache) for (i = 0; i < sizeof s_options / sizeof s_options[0]; i++) {
                const char *v = getenv(s_options[i].name);
                if (v) {
                    size_t n = strlen(v) + 1;
                    s_options[i].value = (char *)malloc(n);
                    if (s_options[i].value) memcpy(s_options[i].value, v, n);
                    else s_options[i].fallback = 1; /* preserve behavior on allocation failure */
                }
            }
            atomic_store_explicit(&s_state, 2, memory_order_release);
        } else {
            while (atomic_load_explicit(&s_state, memory_order_acquire) != 2) { }
        }
    }
    if (!s_cache) return getenv(name);
    for (i = 0; i < sizeof s_options / sizeof s_options[0]; i++)
        if (!strcmp(name, s_options[i].name))
            return s_options[i].fallback ? getenv(name) : s_options[i].value;
    return getenv(name); /* future unlisted probe options retain existing semantics */
}

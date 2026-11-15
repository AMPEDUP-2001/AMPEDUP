#ifndef AMPED_DRAW_ENV_H
#define AMPED_DRAW_ENV_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef AMPED_DRAW_GETENV
#define AMPED_DRAW_GETENV getenv
#endif

/* Snapshot these launch-time options only when explicitly requested. Presence
 * tests retain their existing empty-string semantics; no pointer is retained. */
typedef struct {
    int enabled, depth_source, draw_isolate, occ_ff, depth_readback;
    int draw_trace, draw_trace_frame_set, no_alpha, no_depth;
    uint64_t draw_trace_frame;
} AmpedDrawEnv;

static const AmpedDrawEnv *amped_draw_env(void)
{
    static AmpedDrawEnv c;
    static atomic_int state; /* 0 unread, 1 initializing, 2 published */
    if (atomic_load_explicit(&state, memory_order_acquire) != 2) {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&state, &expected, 1,
                                                    memory_order_acq_rel, memory_order_acquire)) {
            const char *e = AMPED_DRAW_GETENV("AMPED_CACHE_DRAW_ENV");
            c.enabled = e && strcmp(e, "1") == 0;
            if (c.enabled) {
                c.depth_source = AMPED_DRAW_GETENV("AMPED_DEPTH_SOURCE_TRACE") != NULL;
                c.draw_isolate = AMPED_DRAW_GETENV("AMPED_DRAW_ISOLATE") != NULL;
                e = AMPED_DRAW_GETENV("AMPED_OCC_FF_TRANSFORM");
                c.occ_ff = e && strcmp(e, "1") == 0;
                c.depth_readback = AMPED_DRAW_GETENV("AMPED_DEPTH_READBACK") != NULL;
                c.draw_trace = AMPED_DRAW_GETENV("AMPED_DRAW_TRACE") != NULL;
                e = AMPED_DRAW_GETENV("AMPED_DRAW_TRACE_FRAME");
                c.draw_trace_frame_set = e != NULL;
                if (e) c.draw_trace_frame = strtoull(e, NULL, 10);
                c.no_alpha = AMPED_DRAW_GETENV("AMPED_NO_ALPHA_EXPERIMENT") != NULL;
                c.no_depth = AMPED_DRAW_GETENV("AMPED_NO_DEPTH_EXPERIMENT") != NULL;
            }
            atomic_store_explicit(&state, 2, memory_order_release);
        } else {
            while (atomic_load_explicit(&state, memory_order_acquire) != 2) { }
        }
    }
    return &c;
}
#endif

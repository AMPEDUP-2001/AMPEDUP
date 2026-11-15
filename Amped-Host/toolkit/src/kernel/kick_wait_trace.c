/* Opt-in observation of KickOff wait attempts. No synchronization changes. */
#include "kick_wait_trace.h"
#include "platform/host_time.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KICK_WAIT_SAMPLE_STRIDE 64u
#define KICK_WAIT_WINDOW_FLIPS 120u
#define KICK_WAIT_LOG_LIMIT 100u
static atomic_int s_state; /* 0 unread, 1 initializing, 2 off, 3 trace, 4 trace with clear epoch */
static atomic_uint_fast64_t s_buckets[3][6], s_epoch, s_epoch_changed[3];
static _Thread_local uint64_t t_sample_epoch;
static unsigned s_lines, s_flip_count, s_first_flip; /* executor thread only */
static atomic_int s_stopped;
static _Thread_local unsigned t_sample_count;

static int trace_enabled(void)
{
    int state = atomic_load_explicit(&s_state, memory_order_acquire);
    if (state < 2) {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&s_state, &expected, 1,
                                                    memory_order_acq_rel, memory_order_acquire)) {
            const char *value = getenv("AMPED_KICK_WAIT_TRACE");
            state = value && !strcmp(value, "1") ? 3 : 2;
            value = getenv("AMPED_KICK_WAIT_EPOCH");
            if (state == 3 && value && !strcmp(value, "1")) state = 4;
            atomic_store_explicit(&s_state, state, memory_order_release);
        } else {
            do { state = atomic_load_explicit(&s_state, memory_order_acquire); } while (state < 2);
        }
    }
    return state >= 3;
}

uint64_t xbox_KickWaitTraceBegin(void)
{
    if (!trace_enabled() || atomic_load_explicit(&s_stopped, memory_order_relaxed)) return 0;
    /* Deterministic per-thread sampling: periodic work can alias this stride. */
    if (++t_sample_count != KICK_WAIT_SAMPLE_STRIDE) return 0;
    t_sample_count = 0;
    if (atomic_load_explicit(&s_state, memory_order_relaxed) == 4)
        t_sample_epoch = atomic_load_explicit(&s_epoch, memory_order_relaxed);
    return xbox_HostNowNs();
}

void xbox_KickWaitTraceEnd(uint64_t start, unsigned outcome)
{
    static const uint64_t limits[] = {50000, 250000, 1000000, 5000000, 20000000};
    uint64_t end, elapsed;
    unsigned bucket = 0;
    if (!start || outcome > 2) return;
    end = xbox_HostNowNs();
    if (atomic_load_explicit(&s_state, memory_order_relaxed) == 4 &&
        t_sample_epoch != atomic_load_explicit(&s_epoch, memory_order_relaxed))
        atomic_fetch_add_explicit(&s_epoch_changed[outcome], 1, memory_order_relaxed);
    elapsed = end >= start ? end - start : 0;
    while (bucket < 5 && elapsed > limits[bucket]) ++bucket;
    atomic_fetch_add_explicit(&s_buckets[outcome][bucket], 1, memory_order_relaxed);
}

void xbox_KickWaitTraceFlip(uint32_t flip)
{
    uint64_t bins[3][6], changed[3];
    unsigned o, b;
    if (!trace_enabled() || s_lines >= KICK_WAIT_LOG_LIMIT) return;
    if (!s_flip_count) s_first_flip = flip;
    if (++s_flip_count < KICK_WAIT_WINDOW_FLIPS) return;
    s_flip_count = 0;
    for (o = 0; o < 3; ++o)
        for (b = 0; b < 6; ++b)
            bins[o][b] = atomic_exchange_explicit(&s_buckets[o][b], 0, memory_order_relaxed);
    for (o = 0; o < 3; ++o)
        changed[o] = atomic_exchange_explicit(&s_epoch_changed[o], 0, memory_order_relaxed);
    ++s_lines;
    if (s_lines == KICK_WAIT_LOG_LIMIT)
        atomic_store_explicit(&s_stopped, 1, memory_order_relaxed);
    /* One output call per 120 flips. Atomic exchanges are not a group snapshot. */
    (fprintf)(stderr, "[AMPED_KICK_WAIT_TRACE] flips=%u-%u sample_stride=64 bins_us_le_50_250_1000_5000_20000_gt=immediate:%llu,%llu,%llu,%llu,%llu,%llu wake:%llu,%llu,%llu,%llu,%llu,%llu timeout:%llu,%llu,%llu,%llu,%llu,%llu epoch_enabled=%u epoch_changed:%llu,%llu,%llu%s\n",
        s_first_flip, flip,
        (unsigned long long)bins[0][0], (unsigned long long)bins[0][1], (unsigned long long)bins[0][2], (unsigned long long)bins[0][3], (unsigned long long)bins[0][4], (unsigned long long)bins[0][5],
        (unsigned long long)bins[1][0], (unsigned long long)bins[1][1], (unsigned long long)bins[1][2], (unsigned long long)bins[1][3], (unsigned long long)bins[1][4], (unsigned long long)bins[1][5],
        (unsigned long long)bins[2][0], (unsigned long long)bins[2][1], (unsigned long long)bins[2][2], (unsigned long long)bins[2][3], (unsigned long long)bins[2][4], (unsigned long long)bins[2][5],
        atomic_load_explicit(&s_state, memory_order_relaxed) == 4 ? 1u : 0u,
        (unsigned long long)changed[0], (unsigned long long)changed[1], (unsigned long long)changed[2],
        s_lines == KICK_WAIT_LOG_LIMIT ? " (log limit reached)" : "");
}

void xbox_KickWaitTraceCleared(void)
{
    if (trace_enabled() && atomic_load_explicit(&s_state, memory_order_relaxed) == 4 &&
        !atomic_load_explicit(&s_stopped, memory_order_relaxed))
        atomic_fetch_add_explicit(&s_epoch, 1, memory_order_relaxed);
}
#ifdef KICK_WAIT_TRACE_TEST
void xbox_KickWaitTraceTestSetEpoch(uint64_t value) { atomic_store(&s_epoch, value); }
uint64_t xbox_KickWaitTraceTestGetEpoch(void) { return atomic_load(&s_epoch); }
#endif

#include "kick_profile.h"
#include "platform/host_time.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static atomic_int s_state; /* 0 unread, 1 initialising, 2 off, 3 on */
static atomic_uint_fast64_t s_scan_seq, s_scan_count, s_scan_ns;
static atomic_uint_fast64_t s_kicks, s_wait_ns, s_overlap, s_bins[8];

static int kick_prof_on(void)
{
    int s = atomic_load_explicit(&s_state, memory_order_acquire);
    if (s < 2) {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&s_state, &expected, 1,
                                                    memory_order_acq_rel, memory_order_acquire)) {
            const char *e = getenv("AMPED_KICK_PROFILE");
            s = e && !strcmp(e, "1") ? 3 : 2;
            atomic_store_explicit(&s_state, s, memory_order_release);
        } else {
            do { s = atomic_load_explicit(&s_state, memory_order_acquire); } while (s < 2);
        }
    }
    return s == 3;
}

uint64_t xbox_KickProfBegin(uint64_t *scan_sequence)
{
    if (!kick_prof_on()) { *scan_sequence = 0; return 0; }
    *scan_sequence = atomic_load_explicit(&s_scan_seq, memory_order_acquire);
    return xbox_HostNowNs();
}

void xbox_KickProfEnd(uint64_t start_ns, uint64_t scan_sequence)
{
    static const uint64_t limits_ns[7] = {10000, 50000, 100000, 500000, 1000000, 5000000, 20000000};
    uint64_t end, elapsed, seq, n;
    unsigned i;
    if (!start_ns) return;
    end = xbox_HostNowNs();
    elapsed = end >= start_ns ? end - start_ns : 0;
    seq = atomic_load_explicit(&s_scan_seq, memory_order_acquire);
    if ((scan_sequence & 1) || seq != scan_sequence)
        atomic_fetch_add_explicit(&s_overlap, 1, memory_order_relaxed);
    for (i = 0; i < 7 && elapsed > limits_ns[i]; i++) { }
    atomic_fetch_add_explicit(&s_bins[i], 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&s_wait_ns, elapsed, memory_order_relaxed);
    n = atomic_fetch_add_explicit(&s_kicks, 1, memory_order_relaxed) + 1;
    if ((n & 1023u) == 0) {
        (fprintf)(stderr, "[AMPED_KICK_PROFILE] cumulative=%llu wait_ms=%.3f possible_scan_overlap=%llu scans=%llu scan_ms=%.3f bins_us_le_10_50_100_500_1000_5000_20000_gt=",
                  (unsigned long long)n, atomic_load_explicit(&s_wait_ns, memory_order_relaxed) / 1e6,
                  (unsigned long long)atomic_load_explicit(&s_overlap, memory_order_relaxed),
                  (unsigned long long)atomic_load_explicit(&s_scan_count, memory_order_relaxed),
                  atomic_load_explicit(&s_scan_ns, memory_order_relaxed) / 1e6);
        for (i = 0; i < 8; i++)
            (fprintf)(stderr, "%s%llu", i ? "," : "", (unsigned long long)atomic_load_explicit(&s_bins[i], memory_order_relaxed));
        (fprintf)(stderr, " (concurrent cumulative snapshot)\n");
    }
}

uint64_t xbox_KickProfScanBegin(void)
{
    if (!kick_prof_on()) return 0;
    atomic_fetch_add_explicit(&s_scan_seq, 1, memory_order_release);
    return xbox_HostNowNs();
}

void xbox_KickProfScanEnd(uint64_t start_ns)
{
    uint64_t end;
    if (!start_ns) return;
    end = xbox_HostNowNs();
    atomic_fetch_add_explicit(&s_scan_count, 1, memory_order_relaxed);
    if (end >= start_ns) atomic_fetch_add_explicit(&s_scan_ns, end - start_ns, memory_order_relaxed);
    atomic_fetch_add_explicit(&s_scan_seq, 1, memory_order_release);
}

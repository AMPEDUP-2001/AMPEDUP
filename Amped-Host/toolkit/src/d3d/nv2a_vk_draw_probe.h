/* Small, side-effect-free helpers for the opt-in one-frame draw probe. */
#ifndef NV2A_VK_DRAW_PROBE_H
#define NV2A_VK_DRAW_PROBE_H
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

/* Decimal 1-based ordinal or inclusive first-last, bounded to the log cap.
 * Reject partial parses, reversed ranges, overflow and empty endpoints. */
static inline int nvk_draw_probe_range(const char *s, uint32_t cap,
                                       uint32_t *first, uint32_t *last)
{
    char *end;
    unsigned long a, b;
    if (!s || *s < '0' || *s > '9') return 0;
    errno = 0;
    a = strtoul(s, &end, 10);
    if (errno || !a || a > cap) return 0;
    b = a;
    if (*end == '-') {
        s = end + 1;
        if (*s < '0' || *s > '9') return 0;
        errno = 0;
        b = strtoul(s, &end, 10);
        if (errno || b < a || b > cap) return 0;
    }
    if (*end) return 0;
    *first = (uint32_t)a; *last = (uint32_t)b;
    return 1;
}

/* Inclusive backend frames, at most 600 frames. Frame zero is valid. */
static inline int nvk_draw_probe_frames(const char *s, uint64_t *first, uint64_t *last)
{
    char *end;
    unsigned long long a, b;
    if (!s || *s < '0' || *s > '9') return 0;
    errno = 0;
    a = strtoull(s, &end, 10);
    if (errno) return 0;
    b = a;
    if (*end == '-') {
        s = end + 1;
        if (*s < '0' || *s > '9') return 0;
        errno = 0;
        b = strtoull(s, &end, 10);
        if (errno || b < a) return 0;
    }
    if (*end || b - a >= 600) return 0;
    *first = (uint64_t)a; *last = (uint64_t)b;
    return 1;
}

/* Vertex attribute zero: decimal type/size/stride. */
static inline int nvk_draw_probe_layout(const char *s, uint32_t out[3])
{
    uint32_t tmp[3];
    for (unsigned i = 0; i < 3; ++i) {
        unsigned long long n;
        char *end;
        if (!s || *s < '0' || *s > '9') return 0;
        errno = 0;
        n = strtoull(s, &end, 10);
        if (errno || n > UINT32_MAX || (i < 2 ? *end != '/' : *end != '\0')) return 0;
        tmp[i] = (uint32_t)n;
        s = end + (i < 2 ? 1 : 0);
    }
    for (unsigned i = 0; i < 3; ++i) out[i] = tmp[i];
    return 1;
}

/* Amped shadow-caster candidate layout: xyz floats, 4-float weights,
 * 4-byte indices, consistently interleaved in a 44- or 64-byte stream. */
static inline int nvk_draw_probe_skin_layout(uint32_t t0, uint32_t n0, uint32_t s0,
                                             uint32_t t1, uint32_t n1, uint32_t s1,
                                             uint32_t t2, uint32_t n2, uint32_t s2)
{
    return t0 == 2 && n0 == 3 && (s0 == 44 || s0 == 64) &&
           t1 == 2 && n1 == 4 && s1 == s0 &&
           t2 == 4 && n2 == 4 && s2 == s0;
}

/* Zero means no ordinal constraint; bounds are inclusive. */
static inline int nvk_draw_probe_ordinal(uint32_t ordinal, uint32_t first, uint32_t last)
{
    return !first || (ordinal >= first && ordinal <= last);
}

/* A bounded number of same-frame before/after pairs. */
static inline int nvk_draw_probe_count(const char *s, uint32_t *count)
{
    if (!s || s[0] < '1' || s[0] > '8' || s[1]) return 0;
    *count = (uint32_t)(s[0] - '0');
    return 1;
}

static inline int nvk_draw_probe_selected(uint64_t frame, uint64_t wanted,
                                          int depth, uint32_t ordinal,
                                          uint32_t first, uint32_t last)
{
    return frame == wanted && depth && first && ordinal >= first && ordinal <= last;
}
#endif

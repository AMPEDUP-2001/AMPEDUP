#ifndef AMPED_INPUT_SYNC_RANGE_H
#define AMPED_INPUT_SYNC_RANGE_H
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

/* Limit an explicitly requested inclusive flip range to 600 markers. */
static int amped_input_sync_range(const char *s, uint32_t *lo, uint32_t *hi)
{
    char *end;
    unsigned long long a, b;
    if (!s || *s < '0' || *s > '9') return 0;
    errno = 0;
    a = strtoull(s, &end, 10);
    if (errno || *end != '-' || end[1] < '0' || end[1] > '9') return 0;
    s = end + 1;
    errno = 0;
    b = strtoull(s, &end, 10);
    if (errno || *end || a > UINT32_MAX || b > UINT32_MAX || b < a || b - a >= 600) return 0;
    *lo = (uint32_t)a;
    *hi = (uint32_t)b;
    return 1;
}
#endif

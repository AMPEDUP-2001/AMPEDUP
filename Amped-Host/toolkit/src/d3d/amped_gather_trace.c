/* Bounded, sampled Vulkan vertex gather/copy observations. */
#include "amped_gather_trace.h"
#include "platform/host_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STRIDE 64u
#define WINDOW 300u
#define MAX_REPORTS 50u
#define MAX_ATTRS 16u
#define LAYOUT_CAP 64u
#define LAYOUT_TOP 8u
struct layout_attr { uint32_t slot, active, type, size, stride, relative_offset; };
struct layout_key { uint32_t source_inline, n; struct layout_attr attr[MAX_ATTRS]; };
struct layout_entry { struct layout_key key; uint64_t samples, vertices; };
static struct layout_entry layouts[LAYOUT_CAP];
static unsigned layout_used;
static uint64_t layout_overflow;
static int layout_enabled;
static int state = -1; /* Used only from Vulkan executor thread. */
static unsigned cadence, reports;
static uint64_t start_present;
static struct {
    uint64_t samples, gather_ns, copy_ns, vertices, indices, sources[2];
    uint64_t active, defaults, types[8], sizes[5], strides[4];
} stats;
static int enabled(void)
{
    if (state < 0) {
        const char *e = getenv("AMPED_GATHER_TRACE");
        state = e && !strcmp(e, "1");
        e = getenv("AMPED_GATHER_LAYOUT_TRACE");
        layout_enabled = state && e && !strcmp(e, "1");
    }
    return state && reports < MAX_REPORTS;
}

static void layout_record(const struct nv2a_pb_draw *d, const int *slots,
                          unsigned n, uint32_t vertices)
{
    struct layout_key key = {0};
    uint32_t base = UINT32_MAX;
    unsigned i;
    key.source_inline = !!d->inline_data; key.n = n;
    for (i = 0; i < n; ++i) {
        const VertexAttr *a;
        if (slots[i] < 0 || (unsigned)slots[i] >= MAX_ATTRS) continue;
        a = &d->attr[slots[i]];
        if (a->size && a->stride && (d->inline_data || a->offset) && a->offset < base)
            base = a->offset;
    }
    for (i = 0; i < n; ++i) {
        const VertexAttr *a;
        struct layout_attr *out = &key.attr[i];
        out->slot = (uint32_t)slots[i];
        if (slots[i] < 0 || (unsigned)slots[i] >= MAX_ATTRS) continue;
        a = &d->attr[slots[i]];
        if (!a->size || !a->stride || (!d->inline_data && !a->offset)) continue;
        out->active = 1; out->type = a->type; out->size = a->size; out->stride = a->stride;
        out->relative_offset = a->offset - base;
    }
    for (i = 0; i < layout_used; ++i)
        if (!memcmp(&layouts[i].key, &key, sizeof key)) break;
    if (i == LAYOUT_CAP) { ++layout_overflow; return; }
    if (i == layout_used) { layouts[i].key = key; ++layout_used; }
    ++layouts[i].samples; layouts[i].vertices += vertices;
}
static void layout_report(uint64_t present)
{
    unsigned rank, i, j;
    uint64_t shown = 0;
    if (!layout_enabled) return;
    (fprintf)(stderr, "[AMPED_GATHER_LAYOUT_SUMMARY] presents=%llu-%llu used=%u overflow_samples=%llu top_limit=%u\n",
              (unsigned long long)start_present, (unsigned long long)present, layout_used,
              (unsigned long long)layout_overflow, LAYOUT_TOP);
    for (rank = 0; rank < LAYOUT_TOP && rank < layout_used; ++rank) {
        unsigned best = LAYOUT_CAP;
        char line[1800]; int used;
        for (i = 0; i < layout_used; ++i)
            if (!(shown & (1ull << i)) && (best == LAYOUT_CAP || layouts[i].vertices > layouts[best].vertices)) best = i;
        if (best == LAYOUT_CAP) break;
        shown |= 1ull << best;
        used = snprintf(line, sizeof line, "[AMPED_GATHER_LAYOUT] presents=%llu-%llu rank=%u samples=%llu vertices=%llu inline=%u slots=%u attrs=",
                        (unsigned long long)start_present, (unsigned long long)present, rank + 1,
                        (unsigned long long)layouts[best].samples, (unsigned long long)layouts[best].vertices,
                        layouts[best].key.source_inline, layouts[best].key.n);
        for (j = 0; j < layouts[best].key.n && used >= 0 && (size_t)used < sizeof line; ++j) {
            const struct layout_attr *a = &layouts[best].key.attr[j];
            used += snprintf(line + used, sizeof line - (size_t)used, "%s%u:%u:%u:%u:%u:%u", j ? "," : "",
                             a->slot, a->active, a->type, a->size, a->stride, a->relative_offset);
        }
        (fprintf)(stderr, "%s\n", line);
    }
    memset(layouts, 0, sizeof layouts); layout_used = 0; layout_overflow = 0;
}

uint64_t amped_gather_trace_begin(void)
{
    if (!enabled() || ++cadence != STRIDE) return 0;
    cadence = 0;
    return xbox_HostNowNs();
}
uint64_t amped_gather_trace_split(uint64_t start)
{
    return start ? xbox_HostNowNs() : 0;
}
void amped_gather_trace_end(uint64_t start, uint64_t split,
                            const struct nv2a_pb_draw *d, const int *slot,
                            unsigned nf4, uint32_t count, uint32_t nidx)
{
    uint64_t end;
    unsigned i;
    if (!start || !split) return;
    end = xbox_HostNowNs();
    ++stats.samples;
    if (split >= start) stats.gather_ns += split - start;
    if (end >= split) stats.copy_ns += end - split;
    stats.vertices += count; stats.indices += nidx;
    ++stats.sources[d->inline_data ? 1 : 0];
    if (nf4 > MAX_ATTRS) nf4 = MAX_ATTRS;
    if (layout_enabled) layout_record(d, slot, nf4, count);
    for (i = 0; i < nf4; ++i) {
        const VertexAttr *a;
        unsigned sz, st;
        if (slot[i] < 0 || (unsigned)slot[i] >= MAX_ATTRS) continue;
        a = &d->attr[slot[i]];
        if (!a->size || !a->stride || (!d->inline_data && !a->offset)) {
            ++stats.defaults; continue;
        }
        ++stats.active;
        ++stats.types[a->type < 7 ? a->type : 7];
        sz = a->size <= 4 ? a->size - 1 : 4;
        ++stats.sizes[sz];
        st = a->stride <= 16 ? 0 : a->stride <= 32 ? 1 : a->stride <= 64 ? 2 : 3;
        ++stats.strides[st];
    }
}
void amped_gather_trace_flip(uint64_t present)
{
    unsigned i;
    char line[1400];
    int used;
    if (!enabled() || !present || present - start_present < WINDOW) return;
    used = snprintf(line, sizeof line,
        "[AMPED_GATHER_TRACE] presents=%llu-%llu sample_stride=64 samples=%llu gather_ns=%llu copy_ns=%llu vertices=%llu indices=%llu external=%llu inline=%llu active_attrs=%llu default_attrs=%llu types_0to6_other=",
        (unsigned long long)start_present, (unsigned long long)present,
        (unsigned long long)stats.samples, (unsigned long long)stats.gather_ns,
        (unsigned long long)stats.copy_ns, (unsigned long long)stats.vertices,
        (unsigned long long)stats.indices, (unsigned long long)stats.sources[0],
        (unsigned long long)stats.sources[1], (unsigned long long)stats.active,
        (unsigned long long)stats.defaults);
#define ADD_VALUES(label, values, n) do { \
    if (used >= 0 && (size_t)used < sizeof line) \
        used += snprintf(line + used, sizeof line - (size_t)used, "%s", (label)); \
    for (i = 0; i < (n) && used >= 0 && (size_t)used < sizeof line; ++i) \
        used += snprintf(line + used, sizeof line - (size_t)used, "%s%llu", i ? "," : "", (unsigned long long)(values)[i]); \
} while (0)
    ADD_VALUES("", stats.types, 8);
    ADD_VALUES(" sizes_1to4_other=", stats.sizes, 5);
    ADD_VALUES(" strides_le_16_32_64_gt=", stats.strides, 4);
#undef ADD_VALUES
    if (layout_enabled) layout_report(present);
    ++reports;
    (fprintf)(stderr, "%s%s\n", line, reports == MAX_REPORTS ? " (log limit reached)" : "");
    memset(&stats, 0, sizeof stats);
    start_present = present;
}

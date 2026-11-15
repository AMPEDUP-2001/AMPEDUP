#ifndef AMPED_SETTINGS_SAMPLER_PROBE_H
#define AMPED_SETTINGS_SAMPLER_PROBE_H
#include <stdint.h>

typedef struct {
    uint64_t calls, eligible, no_mip, no_linear, scaled, no_cap;
    uint64_t selected_1, selected_2, selected_4, selected_8, selected_16, selected_other;
    uint64_t cached, created, fallback, enhanced_returned, enhanced_fallback;
} amped_sampler_counts;

/* Logarithmic checkpoints bound output over the lifetime of the 64-bit counter;
 * counters continue accumulating between checkpoints and are not phase markers. */
static inline int amped_sampler_report_due(uint64_t calls)
{
    return calls == 1 || (calls >= 4096 && (calls & (calls - 1)) == 0);
}

/* Request counts, not draw counts: callers can discard or replace samplers. */
static inline void amped_sampler_count(amped_sampler_counts *c, int mip, int linear,
                                       int scale, int has_cap, int selected, int outcome)
{
    ++c->calls;
    if (outcome == 0) ++c->cached;
    else if (outcome == 1) ++c->created;
    else ++c->fallback;
    if (selected > 1) {
        if (outcome == 0 || outcome == 1) ++c->enhanced_returned;
        else ++c->enhanced_fallback;
    }
    if (!mip) ++c->no_mip;
    if (!linear) ++c->no_linear;
    if (scale) ++c->scaled;
    if (!has_cap) ++c->no_cap;
    if (mip && linear && !scale && has_cap) ++c->eligible;
    switch (selected) {
    case 1: ++c->selected_1; break;
    case 2: ++c->selected_2; break;
    case 4: ++c->selected_4; break;
    case 8: ++c->selected_8; break;
    case 16: ++c->selected_16; break;
    default: ++c->selected_other; break;
    }
}

static inline int amped_sampler_level_from_key(uint64_t key)
{
    return (int)((key >> 33) & 127u);
}

typedef struct {
    uint64_t draws, active_stages, enhanced_draws, enhanced_stages, null_stages, fallback_stages;
} amped_sampler_draw_counts;

/* `levels` are the final returned sampler levels, 0 for a null handle;
 * `active` excludes unused stages. These are recorded descriptors, not samples. */
static inline void amped_sampler_draw_count(amped_sampler_draw_counts *c,
                                            const int active[4], const int levels[4], const int fallback[4])
{
    int enhanced = 0;
    ++c->draws;
    for (int i = 0; i < 4; ++i) if (active[i]) {
        ++c->active_stages;
        if (levels[i] == 0) ++c->null_stages;
        if (fallback[i]) ++c->fallback_stages;
        if (levels[i] > 1) { ++c->enhanced_stages; enhanced = 1; }
    }
    if (enhanced) ++c->enhanced_draws;
}
#endif

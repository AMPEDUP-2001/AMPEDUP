/* Bounded address arithmetic for the opt-in vertex-content diagnostic. */
#ifndef NV2A_VK_MESH_CONTENT_H
#define NV2A_VK_MESH_CONTENT_H
#include <stdint.h>

/* `base` is relative to the start of a contiguous readable region. Positions
 * are exactly three 32-bit floats. Never wrap and never cross the region. */
static inline int nvk_mesh_position_offset(uint64_t base, uint32_t index,
                                           uint32_t stride, uint64_t region_size,
                                           uint64_t *position)
{
    uint64_t step = (uint64_t)index * stride;
    uint64_t at;
    if (stride < 12 || base > UINT64_MAX - step) return 0;
    at = base + step;
    if (at > region_size || region_size - at < 12) return 0;
    *position = at;
    return 1;
}

/* Evenly sample index slots across the whole draw, including both ends.
 * Call only with sample < min(count, cap) and cap > 0. */
static inline uint32_t nvk_mesh_sample_slot(uint32_t count, uint32_t cap, uint32_t sample)
{
    uint32_t n = count < cap ? count : cap;
    if (n <= 1) return 0;
    return (uint32_t)(((uint64_t)sample * (count - 1)) / (n - 1));
}
#endif

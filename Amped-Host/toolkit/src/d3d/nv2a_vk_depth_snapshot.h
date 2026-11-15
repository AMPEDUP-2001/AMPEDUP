/* Pure eligibility check for the opt-in Vulkan depth snapshot. */
#ifndef NV2A_VK_DEPTH_SNAPSHOT_H
#define NV2A_VK_DEPTH_SNAPSHOT_H
#include <stdint.h>

struct nvk_depth_snapshot_match {
    uint32_t tex_format, tex_mode, tex_valid, tex_addr, tex_w, tex_h;
    uint32_t zeta, current_zeta, zeta_addr, zeta_w, zeta_h;
    uint64_t write_flip, current_flip, write_gen;
    int has_image, transfer_ok, fresh;
};

static inline int nvk_depth_snapshot_matches(const struct nvk_depth_snapshot_match *m)
{
    return m->tex_valid && m->tex_format == 0x2Eu && m->tex_mode == 2 &&
           m->has_image && m->transfer_ok && !m->fresh && m->zeta != m->current_zeta &&
           m->write_gen && m->write_flip == m->current_flip &&
           m->zeta_addr == m->tex_addr && m->zeta_w == m->tex_w && m->zeta_h == m->tex_h;
}
#endif

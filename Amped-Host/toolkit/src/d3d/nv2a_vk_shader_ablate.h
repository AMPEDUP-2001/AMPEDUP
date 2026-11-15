#ifndef NV2A_VK_SHADER_ABLATE_H
#define NV2A_VK_SHADER_ABLATE_H
#include <stdint.h>
/* One requested frame, programmed hash, exact target, no active query. */
static int nvk_shader_ablate_match(int enabled,uint64_t frame,uint64_t wanted,
 uint32_t hash,uint32_t wanted_hash,uint32_t target,uint32_t wanted_target,
 uint32_t w,uint32_t h,int programmed,int query,unsigned used) {
 return enabled && frame==wanted && programmed && hash==wanted_hash && target==wanted_target &&
        w==640 && h==480 && !query && used<512u;
}
#endif

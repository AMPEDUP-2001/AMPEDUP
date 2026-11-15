#include <stdio.h>
#include "../../src/d3d/nv2a_vk_mesh_content.h"

int main(void)
{
    uint64_t out = 0;
    int errors = 0;
#define VALID(base,idx,stride,size,want) do { \
    if (!nvk_mesh_position_offset(base,idx,stride,size,&out) || out != (want)) ++errors; \
} while (0)
#define INVALID(base,idx,stride,size) do { \
    if (nvk_mesh_position_offset(base,idx,stride,size,&out)) ++errors; \
} while (0)
    VALID(0, 0, 12, 12, 0);
    VALID(4, 3, 16, 64, 52);
    VALID(0x1000, 65535, 64, 0x500000, 0x1000 + 65535ull * 64);
    INVALID(0, 0, 11, 32);
    INVALID(4, 3, 16, 63);
    INVALID(UINT64_MAX - 7, 1, 12, UINT64_MAX);
    INVALID(UINT64_MAX - 5, 0, 12, UINT64_MAX);
    INVALID(13, 0, 12, 12);
    if (nvk_mesh_sample_slot(1, 512, 0) != 0 ||
        nvk_mesh_sample_slot(5, 512, 4) != 4 ||
        nvk_mesh_sample_slot(2860, 512, 0) != 0 ||
        nvk_mesh_sample_slot(2860, 512, 511) != 2859) ++errors;
    for (uint32_t i = 1; i < 512; ++i)
        if (nvk_mesh_sample_slot(2860, 512, i) <= nvk_mesh_sample_slot(2860, 512, i - 1)) ++errors;
#undef VALID
#undef INVALID
    printf("mesh content address bounds: %s\n", errors ? "FAIL" : "PASS");
    return errors != 0;
}

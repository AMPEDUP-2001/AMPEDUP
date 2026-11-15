/* Host-only tests for selecting a depth source; no Vulkan device required. */
#include <stdio.h>
#include "../../src/d3d/nv2a_vk_depth_snapshot.h"

int main(void)
{
    const struct nvk_depth_snapshot_match valid = {
        0x2e, 2, 1, 0x80fd0600, 640, 480,
        0x00fd0600, 0x01100000, 0x80fd0600, 640, 480,
        42, 42, 7, 1, 1, 0
    };
    struct nvk_depth_snapshot_match m = valid;
    int errors = !nvk_depth_snapshot_matches(&m);
#define REJECT(field, value) do { \
    m = valid; m.field = (value); \
    if (nvk_depth_snapshot_matches(&m)) { fprintf(stderr, #field " accepted\n"); ++errors; } \
} while (0)
    REJECT(tex_valid, 0);
    REJECT(tex_format, 0x06);
    REJECT(tex_mode, 1);
    REJECT(tex_addr, 0x80fd0610);
    REJECT(tex_w, 639);
    REJECT(tex_h, 479);
    REJECT(current_zeta, 0x00fd0600);
    REJECT(write_flip, 41);
    REJECT(write_gen, 0);
    REJECT(has_image, 0);
    REJECT(transfer_ok, 0);
    REJECT(fresh, 1);
#undef REJECT
    printf("depth source selection: %s\n", errors ? "FAIL" : "PASS");
    return errors != 0;
}

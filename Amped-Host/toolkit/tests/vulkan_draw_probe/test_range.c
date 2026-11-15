#include <stdio.h>
#include "../../src/d3d/nv2a_vk_draw_probe.h"

int main(void)
{
    uint32_t first = 0, last = 0;
    const char *bad[] = {"", "0", "-1", "1-", "-2", "2-1", "1-0", "2049",
                         "1-2049", "1-2x", "1-2-3", " 1", "+1", "42949672960"};
    int errors = 0;
    if (!nvk_draw_probe_range("5-9", 2048, &first, &last) || first != 5 || last != 9)
        ++errors;
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        if (nvk_draw_probe_range(bad[i], 2048, &first, &last)) {
            fprintf(stderr, "accepted invalid range: %s\n", bad[i]);
            ++errors;
        }
    if (!nvk_draw_probe_range("2048", 2048, &first, &last) || first != 2048 || last != 2048)
        ++errors;
    if (!nvk_draw_probe_selected(42, 42, 1, 7, 5, 9) ||
        nvk_draw_probe_selected(41, 42, 1, 7, 5, 9) ||
        nvk_draw_probe_selected(42, 42, 0, 7, 5, 9) ||
        nvk_draw_probe_selected(42, 42, 1, 4, 5, 9) ||
        nvk_draw_probe_selected(42, 42, 1, 10, 5, 9) ||
        nvk_draw_probe_selected(42, 42, 1, 7, 0, 0))
        ++errors;
    if (!nvk_draw_probe_ordinal(1, 0, 0) || !nvk_draw_probe_ordinal(1252, 1252, 1253) ||
        !nvk_draw_probe_ordinal(1253, 1252, 1253) || nvk_draw_probe_ordinal(1251, 1252, 1253) ||
        nvk_draw_probe_ordinal(1254, 1252, 1253) || nvk_draw_probe_ordinal(1253, 1252, 1252)) ++errors;
    {
        uint64_t a, b;
        uint32_t layout[3];
        const char *bad_frames[] = {"", "-1", "1-", "2-1", "0-600", "1-601",
                                    "1-2x", "+1", "18446744073709551616"};
        const char *bad_layouts[] = {"", "2/3", "2/3/", "2/3/-1", "2/3/44x",
                                     "2/3/4294967296", "2/3/44/1"};
        if (!nvk_draw_probe_frames("0", &a, &b) || a != 0 || b != 0 ||
            !nvk_draw_probe_frames("9100-9699", &a, &b) || a != 9100 || b != 9699 ||
            !nvk_draw_probe_frames("18446744073709551615", &a, &b) || a != UINT64_MAX)
            ++errors;
        for (unsigned i = 0; i < sizeof bad_frames / sizeof bad_frames[0]; ++i)
            if (nvk_draw_probe_frames(bad_frames[i], &a, &b)) ++errors;
        if (!nvk_draw_probe_skin_layout(2,3,44,2,4,44,4,4,44) ||
            !nvk_draw_probe_skin_layout(2,3,64,2,4,64,4,4,64) ||
            nvk_draw_probe_skin_layout(2,3,44,2,4,44,4,4,64) ||
            nvk_draw_probe_skin_layout(2,3,48,2,4,48,4,4,48) ||
            nvk_draw_probe_skin_layout(2,3,44,2,4,44,4,3,44)) ++errors;
        {
            uint32_t count;
            const char *bad_counts[] = {"", "0", "9", "10", "-1", "1x", " 1"};
            if (!nvk_draw_probe_count("1", &count) || count != 1 ||
                !nvk_draw_probe_count("8", &count) || count != 8) ++errors;
            for (unsigned i = 0; i < sizeof bad_counts / sizeof bad_counts[0]; ++i)
                if (nvk_draw_probe_count(bad_counts[i], &count)) ++errors;
        }
        if (!nvk_draw_probe_layout("2/3/44", layout) || layout[0] != 2 ||
            layout[1] != 3 || layout[2] != 44) ++errors;
        for (unsigned i = 0; i < sizeof bad_layouts / sizeof bad_layouts[0]; ++i)
            if (nvk_draw_probe_layout(bad_layouts[i], layout)) ++errors;
    }
    printf("draw probe selection: %s\n", errors ? "FAIL" : "PASS");
    return errors != 0;
}

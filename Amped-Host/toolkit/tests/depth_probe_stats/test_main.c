#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* ACTUAL_SUMMARY */

int main(void)
{
    const float values[] = { 1.0f, 0.5f, 0.25f, -0.1f, 1.1f, NAN, INFINITY };
    struct amped_depth_stats s = amped_depth_summarize(values, 7, 1.0f, 1);
    int ok = s.finite == 5 && s.nonfinite == 2 && s.outside == 2 &&
             s.changed == 4 && fabsf(s.min + 0.1f) < 0.00001f &&
             fabsf(s.max - 1.1f) < 0.00001f;
    printf("depth summary finite/range/change accounting: %s\n", ok ? "PASS" : "FAIL");
    s = amped_depth_summarize(values, 3, 1.0f, 0);
    int no_baseline = s.finite == 3 && s.changed == 0 && s.min == 0.25f && s.max == 1.0f;
    printf("depth summary without clear baseline: %s\n", no_baseline ? "PASS" : "FAIL");
    return !(ok && no_baseline);
}

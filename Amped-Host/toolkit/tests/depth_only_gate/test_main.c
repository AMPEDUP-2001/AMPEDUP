#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ACTUAL_PREDICATE */

static int check(const char *name, int actual, int expected)
{
    int ok = actual == expected;
    printf("%s: %s\n", name, ok ? "PASS" : "FAIL");
    return !ok;
}

int main(void)
{
    int failures = 0;
    _putenv_s("AMPED_DEPTH_ONLY_EXPERIMENT", "");
    failures += check("default disabled", depth_only_wanted(1, 0, ~0u, 1, 1), 0);
    _putenv_s("AMPED_DEPTH_ONLY_EXPERIMENT", "1");
    failures += check("depth clear", depth_only_wanted(1, 0, ~0u, 1, 1), 1);
    failures += check("stencil clear", depth_only_wanted(1, 0, ~0u, 2, 1), 1);
    failures += check("combined color clear excluded", depth_only_wanted(1, 0, ~0u, 0xF1, 1), 0);
    failures += check("color-only clear excluded", depth_only_wanted(1, 0, ~0u, 0xF0, 1), 0);
    failures += check("zero-color depth draw", depth_only_wanted(0, 0, ~0u, 0, 1), 1);
    failures += check("enabled color channel excluded", depth_only_wanted(0, 0, 0, 0, 1), 0);
    failures += check("nonzero color address excluded", depth_only_wanted(0, 0x10000, ~0u, 0, 1), 0);
    failures += check("no depth/stencil excluded", depth_only_wanted(0, 0, ~0u, 0, 0), 0);
    return failures != 0;
}

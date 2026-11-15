/* Use the game's configuration parser for the isolated HUD selector. */
#include "recomp_cfg.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    char error[512];
    recomp_cfg *cfg;
    recomp_cfg_type type;
    int wide;
    if (argc != 2) {
        fputs("expected settings path\n", stderr);
        return 2;
    }
    cfg = recomp_cfg_load_file_flags(argv[1], RCFG_REOPEN_TABLES, error, sizeof error);
    if (!cfg) {
        fprintf(stderr, "settings rejected: %s\n", error);
        return 2;
    }
    type = recomp_cfg_type_of(cfg, "display.amped_native_wide");
    if (type != RCFG_NONE && type != RCFG_BOOL) {
        fputs("amped_native_wide must be boolean\n", stderr);
        recomp_cfg_free(cfg);
        return 2;
    }
    wide = recomp_cfg_bool(cfg, "display.amped_native_wide", 0);
    recomp_cfg_free(cfg);
    puts(wide ? "wide" : "standard");
    return 0;
}

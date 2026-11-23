/* Use the game's configuration parser for the isolated HUD selector. */
#include "recomp_cfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *profile_language(const recomp_cfg *cfg)
{
    static const char *const names[] = { NULL, "english", NULL, "german", "french", "spanish", "italian" };
    const char *text = recomp_cfg_string(cfg, "game.language", NULL);
    long long id = recomp_cfg_int(cfg, "game.language", 0);
    if (text) {
        for (int i = 1; i <= 6; ++i)
            if (names[i] && strcmp(text, names[i]) == 0) return names[i];
        char *end;
        id = strtol(text, &end, 10);
        while (isspace((unsigned char)*end)) ++end;
        if (*end) return "english";
    }
    return id >= 1 && id <= 6 && names[id] ? names[id] : "english";
}

int main(int argc, char **argv)
{
    char error[512];
    recomp_cfg *cfg;
    recomp_cfg_type type;
    int wide;
    int language = argc == 3 && strcmp(argv[1], "--language-info") == 0;
    if (argc != 2 && !language) {
        fputs("expected [--language-info] settings-path\n", stderr);
        return 2;
    }
    cfg = recomp_cfg_load_file_flags(argv[argc-1], RCFG_REOPEN_TABLES, error, sizeof error);
    if (!cfg) {
        fprintf(stderr, "settings rejected: %s\n", error);
        return 2;
    }
    if (language) {
        printf("%d %s\n", recomp_cfg_line(cfg, "game.language"), profile_language(cfg));
        recomp_cfg_free(cfg);
        return 0;
    }
    type = recomp_cfg_type_of(cfg, "display.amped_native_wide");
    if (type != RCFG_NONE && type != RCFG_BOOL) {
        fputs("amped_native_wide must be boolean\n", stderr);
        recomp_cfg_free(cfg);
        return 2;
    }
    type = recomp_cfg_type_of(cfg, "display.amped_ultrawide");
    if (type != RCFG_NONE && type != RCFG_BOOL) {
        fputs("amped_ultrawide must be boolean\n", stderr);
        recomp_cfg_free(cfg);
        return 2;
    }
    wide = recomp_cfg_bool(cfg, "display.amped_native_wide", 0)
        || recomp_cfg_bool(cfg, "display.amped_ultrawide", 0);
    recomp_cfg_free(cfg);
    puts(wide ? "wide" : "standard");
    return 0;
}

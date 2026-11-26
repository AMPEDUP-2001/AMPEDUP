#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int amped_kick_wait_enabled_v23(void);
void amped_kick_preset_v23(int enabled);
static int calls, mode_seen = -1;
static int local_calls;
static uint32_t local_site;
void xbox_PacingSetMode(int mode) { calls++; mode_seen = mode; }
void xbox_PacingSetLocalSpinSite(uint32_t va) { local_calls++; local_site=va; }
uint32_t xbox_PacingLocalSpinSite(void) { return local_site; }

int main(int argc, char **argv)
{
    int enabled = argc > 1 && !strcmp(argv[1], "on");
    int pacing = argc > 1 && !strcmp(argv[1], "pacing");
    int local = argc > 1 && !strcmp(argv[1], "local");
    int potato = argc > 1 && !strcmp(argv[1], "potato");
    amped_kick_preset_v23(potato);
#ifdef _WIN32
    _putenv_s("AMPED_KICK_WAIT_SLEEP", enabled ? "1" : "0");
    _putenv_s("AMPED_KICK_PACING_SLEEP", pacing ? "1" : "0");
    _putenv_s("AMPED_KICK_LOCAL_WAIT", potato ? "" : local ? "1" : "0");
#else
    setenv("AMPED_KICK_WAIT_SLEEP", enabled ? "1" : "0", 1);
    setenv("AMPED_KICK_PACING_SLEEP", pacing ? "1" : "0", 1);
    setenv("AMPED_KICK_LOCAL_WAIT", local ? "1" : "0", 1);
    if (potato) unsetenv("AMPED_KICK_LOCAL_WAIT");
#endif
    local = local || potato;
    if (amped_kick_wait_enabled_v23() != (enabled || local) || amped_kick_wait_enabled_v23() != (enabled || local))
        return 1;
    if (calls != (enabled || pacing) || ((enabled || pacing) && mode_seen != 1)) return 2;
    if (local_calls != local || (local && local_site != 0x000FE920u)) return 3;
    (fprintf)(stderr, "KickOff wait %s: PASS\n", enabled ? "on" : pacing ? "pacing-only" : "off");
    return 0;
}

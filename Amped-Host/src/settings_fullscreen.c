#include "settings_fullscreen.h"

int amped_FullscreenSync(amped_fullscreen_state *state, int requested,
                         int actual, int (*set_mode)(void *, int), void *ctx)
{
    requested = requested != 0;
    actual = actual != 0;
    if (state->last_requested == requested)
        return 0;
    state->last_requested = requested;
    if (actual == requested) {
        state->failed = 0;
        return 1;
    }
    if (!set_mode || set_mode(ctx, requested) != 0) {
        state->failed = 1;
        return -1;
    }
    state->failed = 0;
    return 1;
}

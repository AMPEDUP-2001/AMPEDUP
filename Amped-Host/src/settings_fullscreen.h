#ifndef AMPED_SETTINGS_FULLSCREEN_H
#define AMPED_SETTINGS_FULLSCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int last_requested; /* Initialize to -1 before the first poll. */
    int failed;
} amped_fullscreen_state;

/* Attempt at most once per requested value. Return -1 on failure, 1 when a
 * request is satisfied, or 0 when already attempted. The callback returns
 * zero on success, like SDL_SetWindowFullscreen. */
int amped_FullscreenSync(amped_fullscreen_state *state, int requested,
                         int actual, int (*set_mode)(void *, int), void *ctx);

#ifdef __cplusplus
}
#endif
#endif

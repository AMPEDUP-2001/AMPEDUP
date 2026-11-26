#include "settings_fullscreen.h"
#include <SDL.h>
#include <stdio.h>

/* SDL owns the temporary display change and restores the desktop on exit.
 * Enumerate this monitor's modes so a refresh request cannot lower resolution. */
static int matching_mode(int display, int cap, int potato, SDL_DisplayMode *mode)
{
    if (SDL_GetDesktopDisplayMode(display, mode) < 0)
        return -1;
    if (cap == 0)
        return 0;
    int best = 0;
    /* At 30 Hz a missed refresh holds a frame for 67 ms. Prefer a second
     * presentation opportunity when the panel offers a compatible rate. */
    const int preferred = potato && cap == 30 ? 60 : cap;
    int count = SDL_GetNumDisplayModes(display);
    if (count < 0)
        return -1;
    for (int i = 0; i < count; ++i) {
        SDL_DisplayMode candidate;
        if (SDL_GetDisplayMode(display, i, &candidate) < 0)
            continue;
        int hz = candidate.refresh_rate;
        if (candidate.w != mode->w || candidate.h != mode->h || candidate.format != mode->format ||
            hz < cap || hz % cap != 0)
            continue;
        if (!best || (hz >= preferred && best < preferred) ||
            ((hz >= preferred) == (best >= preferred) && hz < best)) {
            *mode = candidate;
            best = hz;
        }
    }
    return 0;
}

int amped_ApplyFullscreen(SDL_Window *window, int enabled, int match_hz, int potato,
                          char *notice, size_t notice_size)
{
    if (!enabled || match_hz < 0)
        return SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

    int display = SDL_GetWindowDisplayIndex(window);
    SDL_DisplayMode mode, actual;
    if (display >= 0 && matching_mode(display, match_hz, potato, &mode) == 0 &&
        SDL_SetWindowDisplayMode(window, &mode) == 0 &&
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) == 0 &&
        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN &&
        SDL_GetCurrentDisplayMode(display, &actual) == 0 &&
        actual.w == mode.w && actual.h == mode.h && actual.refresh_rate == mode.refresh_rate) {
        XR_NOTICE("[AMPED_DISPLAY] fullscreen display=%d size=%dx%d cap=%d refresh=%d Hz\n",
                display, actual.w, actual.h, match_hz, actual.refresh_rate);
        if (match_hz && (actual.refresh_rate < match_hz || actual.refresh_rate % match_hz))
            snprintf(notice, notice_size, "No matching refresh for %d FPS. Using the desktop rate of %d Hz.",
                     match_hz, actual.refresh_rate);
        return 0;
    }
    snprintf(notice, notice_size, "Display mode unavailable. Using borderless fullscreen.");
    XR_NOTICE("[AMPED_DISPLAY] fullscreen request failed, cap=%d: %s\n", match_hz, SDL_GetError());
    return SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

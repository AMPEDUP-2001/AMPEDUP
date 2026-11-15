#include "settings_display.h"
#include "xbox_settings.h"
#include "nv2a_backend_common.h"

#include <stdio.h>
#include <stdlib.h>

void amped_SettingsApplyInitialDisplay(void)
{
    static const int aniso_levels[] = { 1, 2, 4, 8, 16 };
    int aniso_choice = (int)xbox_SettingValue(XS_GRAPHICS_ANISO);
    struct nv2a_host_opts options = *nv2a_host_opts();
    if (aniso_choice < 0 || aniso_choice > 4) aniso_choice = 0;
    options.fullscreen = xbox_SettingValue(XS_DISPLAY_FULLSCREEN) != 0.0;
    options.present_filter = (int)xbox_SettingValue(XS_DISPLAY_FILTER);
#if defined(AMPED_RENDER_SCALE_EXPERIMENT)
    /* This title does not call xbox_enhance_init.  Select the tested integer
     * scale explicitly, before any render targets are created. */
    {
        int scale = (int)xbox_SettingValue(XS_GRAPHICS_RENDER_SCALE);
#if defined(AMPED_RENDER_SCALE_6_EXPERIMENT)
        if (scale < 1 || scale > 6) scale = 1;
#else
        if (scale < 1 || scale > 4) scale = 1;
#endif
        options.render_scale = (unsigned)scale;
        options.render_scale_half = 0;
    }
#endif
    nv2a_host_opts_set(&options);
    nv2a_present_set_overscan((int)xbox_SettingValue(XS_DISPLAY_OVERSCAN));
    nv2a_host_set_aniso(aniso_levels[aniso_choice]);
    if (getenv("AMPED_SETTINGS_STARTUP_TRACE")) {
        const struct nv2a_host_opts *active = nv2a_host_opts();
        (fprintf)(stderr, "[AMPED_SETTINGS_STARTUP] fullscreen=%d filter=%d scale=%u overscan=%d requested_aniso=%d\n",
                  active->fullscreen, active->present_filter, active->render_scale,
                  (int)xbox_SettingValue(XS_DISPLAY_OVERSCAN), aniso_levels[aniso_choice]);
    }
}

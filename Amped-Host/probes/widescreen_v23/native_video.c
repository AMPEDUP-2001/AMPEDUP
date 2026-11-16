/* Isolated diagnostic: request the Xbox dashboard widescreen bit at startup. */
#include "kernel.h"
#include "nv2a_backend_common.h"
#include <windows.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(AMPED_NATIVE_WIDE_SETTING_V23)
#include "../../toolkit/src/overlay/xbox_settings.h"
int amped_native_wide_setting_active_v23;
#endif
static int exactly_one(const char *key){const char *v=getenv(key);return v&&!strcmp(v,"1");}
void amped_native_video_startup_v23(void) {
    DWORD win=GetLastError();int err=errno;
    int wide=exactly_one("AMPED_NATIVE_WIDE_TEST");
#if defined(AMPED_ULTRAWIDE_TRIAL_V23)
    int ultrawide = 0;
#endif
#if defined(AMPED_NATIVE_WIDE_SETTING_V23)
    amped_native_wide_setting_active_v23=xbox_SettingValue(XS_AMPED_NATIVE_WIDE)!=0.0;
#if defined(AMPED_ULTRAWIDE_TRIAL_V23)
    ultrawide = xbox_SettingValue(XS_AMPED_ULTRAWIDE) != 0.0;
    amped_native_wide_setting_active_v23 |= ultrawide;
#endif
    wide=wide||amped_native_wide_setting_active_v23;
#endif
    if(wide) {
        ULONG old=xbox_GetVideoSettings();struct nv2a_host_opts o=*nv2a_host_opts();
        xbox_SetVideoSettings(old|XC_VIDEO_AV_WIDESCREEN,xbox_HalBootSMCVideoMode);
        o.present_aspect=NV2A_ASPECT_16_9;nv2a_host_opts_set(&o);
#if defined(AMPED_ULTRAWIDE_TRIAL_V23)
        extern void amped_ultrawide_trial_init(int);
        amped_ultrawide_trial_init(ultrawide);
#endif
        (fprintf)(stderr,"[AMPED_NATIVE_VIDEO] dashboard_widescreen=1 xc_video=%08lX host_present=16:9 experimental=1\n",(unsigned long)xbox_GetVideoSettings());
    }
    errno=err;SetLastError(win);
}
void amped_native_video_flags_v23(unsigned flags) {
    DWORD win=GetLastError();int err=errno;static volatile LONG lines;
    if(exactly_one("AMPED_NATIVE_VIDEO_TRACE") && InterlockedIncrement(&lines)<=8)
        (fprintf)(stderr,"[AMPED_NATIVE_VIDEO] guest_video_flags=%08X source=8F8F3\n",flags);
    errno=err;SetLastError(win);
}
void amped_native_video_aspect_v23(unsigned bits) {
    DWORD win=GetLastError();int err=errno;static volatile LONG lines;
    if(exactly_one("AMPED_NATIVE_VIDEO_TRACE") && InterlockedIncrement(&lines)<=8)
        (fprintf)(stderr,"[AMPED_NATIVE_VIDEO] guest_global_aspect_bits=%08X source=8F908\n",bits);
    errno=err;SetLastError(win);
}

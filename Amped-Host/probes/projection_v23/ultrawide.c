/* Opt-in trial. The HUD shader and menu state are shared by some scenes.
 * This probe compares draw and presentation behavior in a single run. */
#include "nv2a_backend_common.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TRIAL_ASPECT (64.0 / 27.0)
#define NATIVE_ASPECT_BITS UINT32_C(0x3fe38e39)
#define TRIAL_ASPECT_BITS UINT32_C(0x4017b426)
#define FLIP_SLOTS 64
extern ptrdiff_t g_xbox_mem_offset;
static int s_camera_trial;
static uint64_t s_wide_flip[FLIP_SLOTS];

static void *guest_address(uint32_t va, size_t size)
{
    uint64_t end = (uint64_t)va + size;
    if (g_xbox_mem_offset &&
        ((va >= 0x10000 && end <= 0x04000000) ||
         (va >= 0x80000000 && end <= 0x84000000)))
        return (void *)((uintptr_t)g_xbox_mem_offset + va);
    return NULL;
}

static int guest_read(uint32_t va, void *out, size_t size)
{
    SIZE_T copied;
    const void *p = guest_address(va, size);
    return p && ReadProcessMemory(GetCurrentProcess(), p, out, size, &copied) && copied == size;
}

int amped_ultrawide_trial_hud(uint32_t flip)
{
    static uint32_t previous_flip = UINT32_MAX;
    static unsigned consecutive;
    static int wide;
    if (!s_camera_trial)
        return 0;
    if (previous_flip == flip)
        return wide;
    const int next_flip = flip == previous_flip + 1u;
    previous_flip = flip;
    uint32_t manager, name, engine = 0, camera = 0, aspect = 0;
    char label[32] = {0};
    static char last[sizeof label];
    static int last_wide;
    if (guest_read(0x19e8b0, &manager, 4) && manager <= UINT32_MAX - 0x38 &&
        guest_read(manager + 0x38, &name, 4))
        guest_read(name, label, sizeof(label) - 1);
    if (guest_read(0x18f7c8, &engine, 4) && engine <= UINT32_MAX - 0x10 &&
        guest_read(engine + 0x10, &camera, 4) && camera <= UINT32_MAX - 0xec)
        guest_read(camera + 0xec, &aspect, 4);
    const int eligible = aspect == TRIAL_ASPECT_BITS && !_stricmp(label, "HUD");
    if (!eligible)
        consecutive = 0;
    else if (!next_flip)
        consecutive = 1;
    else if (consecutive < 3)
        ++consecutive;
    /* The title briefly activates Hud before the drop-point menu.
       Wait for a continuous HUD sequence before expanding the frame. */
    wide = consecutive == 3;
    if (wide)
        __atomic_store_n(&s_wide_flip[flip % FLIP_SLOTS], (uint64_t)flip + 1u, __ATOMIC_RELEASE);
    if (strcmp(last, label) || last_wide != wide) {
        (fprintf)(stderr, "[AMPED_ULTRAWIDE] hud draw for flip=%u 21:9=%d menu=%s camera_aspect=%08x\n",
                  flip, wide, label, aspect);
        memcpy(last, label, sizeof last);
        last_wide = wide;
    }
    return wide;
}

static double present_aspect(uint32_t flip)
{
    uint64_t tag = __atomic_load_n(&s_wide_flip[flip % FLIP_SLOTS], __ATOMIC_ACQUIRE);
    int wide = tag == (uint64_t)flip + 1u;
    static int last_wide;
    if (wide != last_wide) {
        (fprintf)(stderr, "[AMPED_ULTRAWIDE] present flip=%u 21:9=%d\n", flip, wide);
        last_wide = wide;
    }
    return wide ? TRIAL_ASPECT : 0.0;
}

void amped_ultrawide_trial_init(int configured)
{
    const char *mode = getenv("AMPED_ULTRAWIDE_TRIAL");
    int window_only = mode && !strcmp(mode, "window");
    s_camera_trial = !window_only && (configured || (mode && !strcmp(mode, "camera")));
    if (!s_camera_trial && !window_only)
        return;
    nv2a_present_window_aspect(TRIAL_ASPECT);
    if (s_camera_trial)
        nv2a_present_frame_aspect_fn(present_aspect);
}

void amped_ultrawide_trial_projection(unsigned esp)
{
    if (!s_camera_trial)
        return;
    uint32_t *args = guest_address(esp, 4 * sizeof(uint32_t));
    if (!args || args[3] != NATIVE_ASPECT_BITS)
        return;
    /* These constructors build a course camera and then use its +EC
       aspect to compute the culling frustum at 9D350. */
    switch (args[0]) {
    case 0x6c7f2: case 0x6dd26: case 0x6fc9b: case 0x6ffa3:
    case 0x701d7: case 0x70403: case 0x70637:
        break;
    default:
        return;
    }
    if (args[1] > UINT32_MAX - 0x8c)
        return;
    uint32_t *camera_aspect = guest_address(args[1] + 0x8c, sizeof(uint32_t));
    if (!camera_aspect || *camera_aspect != NATIVE_ASPECT_BITS)
        return;
    const float wide = (float)TRIAL_ASPECT;
    memcpy(camera_aspect, &wide, sizeof(wide));
    memcpy(args + 3, &wide, sizeof(wide));
    static unsigned reports;
    if (reports++ < 24)
        (fprintf)(stderr, "[AMPED_ULTRAWIDE] expanded course projection at %08x\n", args[0]);
}

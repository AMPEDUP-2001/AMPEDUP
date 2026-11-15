/* v23-only read-only probe: Xbox XINPUT_STATE has four-byte packet number,
 * two-byte digital mask, then A/B/X/Y/Black/White/L/R analog values. */
#include "probe_env_v23.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
extern ptrdiff_t g_xbox_mem_offset;

/* A new, per-session marker file can delay tracing until a course is observed.
 * Each thread latches arming on its next hook; no guest state is modified. */
int amped_a_trace_armed(void)
{
    static __thread int armed;
    static __thread ULONGLONG next_check;
    const char *flag = amped_probe_getenv("AMPED_A_GAME_TRACE"), *path;
    DWORD attr;
    if (!flag || strcmp(flag, "1")) return 0;
    if (armed) return 1;
    path = amped_probe_getenv("AMPED_A_GAME_ARM_FILE");
    if (path && *path) {
        ULONGLONG now = GetTickCount64();
        if (now < next_check) return 0;
        next_check = now + 250u;
        attr = GetFileAttributesA(path);
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) return 0;
    }
    armed = 1;
    (fprintf)(stderr, "[AMPED_A_ARMED] tick_ms=%llu thread=%lu\n",
              (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentThreadId());
    return 1;
}

void amped_trace_getstate_v23(uint32_t handle, uint32_t out, uint32_t result)
{
    static __thread uint32_t last_result = UINT32_MAX, last_digital = UINT32_MAX;
    static __thread unsigned last_black = 256, polls;
    static __thread int last_lx = 65536, last_ly = 65536, last_rx = 65536, last_ry = 65536;
    static __thread unsigned last_a = 256, last_score_x = 256, score_x_logged;
    static __thread unsigned sync_logged;
    static __thread ULONGLONG last_log_ms;
    uint32_t digital = UINT32_MAX;
    unsigned black = 256, a = 256, x = 256;
    int lx = 65536, ly = 65536, rx = 65536, ry = 65536;
    ULONGLONG now;
    if (!amped_probe_getenv("AMPED_GETSTATE_TRACE")) return;
    if (!result && ((out >= 0x10000u && out <= 0x03FFFFEAu) ||
                    (out >= 0x80000000u && out <= 0x83FFFFEAu))) {
        const uint8_t *p = (const uint8_t *)((uintptr_t)g_xbox_mem_offset + out);
        digital = (uint32_t)p[4] | ((uint32_t)p[5] << 8);
        black = p[10];
        a = p[6];
        x = p[8]; /* XInput bAnalogButtons[X], see input_map.c */
        lx = (int16_t)((uint16_t)p[14] | ((uint16_t)p[15] << 8));
        ly = (int16_t)((uint16_t)p[16] | ((uint16_t)p[17] << 8));
        rx = (int16_t)((uint16_t)p[18] | ((uint16_t)p[19] << 8));
        ry = (int16_t)((uint16_t)p[20] | ((uint16_t)p[21] << 8));
    }
    ++polls;
    now = GetTickCount64();
    if (x != 256 && x != last_score_x && score_x_logged < 64) {
        const char *score = amped_probe_getenv("AMPED_A_SCORE_TRACE");
        if (score && !strcmp(score, "1") && amped_a_trace_armed()) {
            (fprintf)(stderr, "[AMPED_SCORE_X] tick_ms=%llu thread=%lu handle=%08X x=%u polls=%u\n",
                      (unsigned long long)now, (unsigned long)GetCurrentThreadId(), handle, x, polls);
            last_score_x = x;
            ++score_x_logged;
        }
    }
    if (a != 256 && a != last_a && sync_logged < 64) {
        const char *sync = amped_probe_getenv("AMPED_INPUT_SYNC_TRACE");
        if (sync && !strcmp(sync, "1")) {
            (fprintf)(stderr, "[AMPED_INPUT_SYNC_A] tick_ms=%llu thread=%lu a=%u polls=%u\n",
                      (unsigned long long)now, (unsigned long)GetCurrentThreadId(), a, polls);
            ++sync_logged;
        }
    }
    if (result != last_result || digital != last_digital || a != last_a || black != last_black || lx != last_lx || ly != last_ly || rx != last_rx || ry != last_ry || now - last_log_ms >= 2000) {
        (fprintf)(stderr, "[AMPED_GETSTATE_V23] polls=%u handle=%08X out=%08X status=%08X digital=%08X a=%u black=%u lx=%d ly=%d rx=%d ry=%d\n",
                polls, handle, out, result, digital, a, black, lx, ly, rx, ry);
        last_result = result; last_digital = digital; last_a = a; last_black = black; last_lx = lx; last_ly = ly; last_rx = rx; last_ry = ry; last_log_ms = now;
    }
}

/* This hook runs immediately after the game's threshold/edge conversion. */
void amped_trace_input_edge_v23(uint32_t object, uint32_t black, uint32_t threshold,
                                uint32_t current, uint32_t previous, uint32_t edge)
{
    static __thread uint32_t last_black[4] = {256, 256, 256, 256};
    static __thread uint32_t last_mask[4] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    static __thread uint32_t last_a[4] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    static __thread unsigned a_logged;
    uint32_t slot, mask, selected, mode, binding_a, binding_b, binding_c;
    const uint8_t *base = (const uint8_t *)(uintptr_t)g_xbox_mem_offset;
    if (object < 0x0019E9A8u || object >= 0x0019E9A8u + 4u * 0x10Cu ||
        (object - 0x0019E9A8u) % 0x10Cu) return;
    slot = (object - 0x0019E9A8u) / 0x10Cu;
    {
        uint32_t a = current & 0x100u;
        if (a_logged < 64 && a != last_a[slot] && amped_a_trace_armed()) {
            memcpy(&selected, base + 0x19E9A0u, sizeof selected);
            (fprintf)(stderr, "[AMPED_A_EDGE] tick_ms=%llu thread=%lu slot=%u selected=%u current=%08X previous=%08X rising=%08X\n",
                      (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentThreadId(),
                      slot, selected, current, previous, edge);
#if defined(AMPED_A_ACTION_PROBE_V23)
            if (slot == selected && ((current ^ previous) & 0x100u)) {
                extern void amped_action_edge_checkpoint_v23(uint32_t, uint32_t, uint32_t);
                amped_action_edge_checkpoint_v23(current, previous, edge);
            }
#endif
            last_a[slot] = a;
            ++a_logged;
        }
    }
    {
        static __thread uint32_t last_x[4] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
        static __thread unsigned x_count;
        const char *xflag = amped_probe_getenv("AMPED_X_CONSUMER_TRACE");
        uint32_t x = current & 0x400u;
        if (xflag && !strcmp(xflag,"1") && amped_a_trace_armed() && x_count < 64 && last_x[slot] != x) {
            memcpy(&selected, base + 0x19E9A0u, sizeof selected);
            (fprintf)(stderr,"[AMPED_X_MASK_EDGE] tick_ms=%llu thread=%lu slot=%u selected=%u current=%08X previous=%08X rising=%08X\n",(unsigned long long)GetTickCount64(),(unsigned long)GetCurrentThreadId(),slot,selected,current,previous,edge);
            last_x[slot]=x; ++x_count;
        }
    }
    if (!amped_probe_getenv("AMPED_GETSTATE_TRACE")) return;
    memcpy(&selected, base + 0x19E9A0u, sizeof selected);
    memcpy(&mode, base + 0x19EF20u, sizeof mode);
    memcpy(&binding_a, base + 0x18E02Cu, sizeof binding_a);
    memcpy(&binding_b, base + 0x18DFE8u, sizeof binding_b);
    memcpy(&binding_c, base + 0x19E928u, sizeof binding_c);
    mask = ((current & 0x1000u) ? 1u : 0u) |
           ((previous & 0x1000u) ? 2u : 0u) | ((edge & 0x1000u) ? 4u : 0u);
    if (black != last_black[slot] || mask != last_mask[slot]) {
        (fprintf)(stderr, "[AMPED_INPUT_EDGE_V23] slot=%u object=%08X black=%u threshold=%u current=%08X previous=%08X rising=%08X selected=%u mode=%08X map_a=%08X map_b=%08X map_c=%08X\n",
                  slot, object, black, threshold, current, previous, edge, selected, mode, binding_a, binding_b, binding_c);
        last_black[slot] = black; last_mask[slot] = mask;
    }
}

/* Bounded read-only button-edge query trace. A mask may cover several buttons;
 * query true means any requested bit matched, not necessarily Black alone. */
void amped_trace_button_v23(uint32_t kind, uint32_t mask, uint32_t requested,
                            uint32_t ret, uint32_t result, uint32_t selected_before, uint32_t mode_before)
{
    static __thread unsigned logged;
    static __thread unsigned a_logged;
    static __thread struct { uint32_t kind, mask, ret; } a_seen[64];
    static __thread uint32_t last_kind = UINT32_MAX, last_mask, last_slot, last_ret, last_result, last_selected, last_mode;
    static __thread ULONGLONG last_ms;
    const uint8_t *base = (const uint8_t *)(uintptr_t)g_xbox_mem_offset;
    uint32_t selected, mode;
    ULONGLONG now;
    {
        unsigned i;
        if ((mask & 0x100u) && result && a_logged < 64 && amped_a_trace_armed()) {
            for (i = 0; i < a_logged; ++i)
                if (a_seen[i].kind == kind && a_seen[i].mask == mask && a_seen[i].ret == ret) break;
            if (i < a_logged) goto a_query_done;
            memcpy(&selected, base + 0x19E9A0u, sizeof selected);
            memcpy(&mode, base + 0x19EF20u, sizeof mode);
            (fprintf)(stderr, "[AMPED_A_QUERY] tick_ms=%llu thread=%lu kind=%u mask=%08X requested=%08X caller=%08X result=%u selected_before=%u selected_after=%u mode_before=%08X mode_after=%08X\n",
                      (unsigned long long)GetTickCount64(), (unsigned long)GetCurrentThreadId(),
                      kind, mask, requested, ret, result, selected_before, selected, mode_before, mode);
            a_seen[a_logged].kind = kind;
            a_seen[a_logged].mask = mask;
            a_seen[a_logged].ret = ret;
            ++a_logged;
        }
    }
a_query_done:
    if (!amped_probe_getenv("AMPED_GETSTATE_TRACE") || !(mask & 0x1000u) || logged >= 256) return;
    memcpy(&selected, base + 0x19E9A0u, sizeof selected);
    memcpy(&mode, base + 0x19EF20u, sizeof mode);
    now = GetTickCount64();
    if (kind != last_kind || mask != last_mask || requested != last_slot || ret != last_ret ||
        result != last_result || selected != last_selected || mode != last_mode || now - last_ms >= 2000) {
        (fprintf)(stderr, "[AMPED_BUTTON_QUERY_V23] kind=%u mask=%08X requested=%08X selected_before=%u mode_before=%08X selected_after=%u mode_after=%08X caller=%08X result=%u\n",
                  kind, mask, requested, selected_before, mode_before, selected, mode, ret, result);
        last_kind = kind; last_mask = mask; last_slot = requested; last_ret = ret;
        last_result = result; last_selected = selected; last_mode = mode; last_ms = now;
        if (++logged == 256) (fprintf)(stderr, "[AMPED_BUTTON_QUERY_V23] per-thread cap reached\n");
    }
}

/* Observe only the camera-cycle calls made after the gameplay button query. */
void amped_trace_camera_v23(uint32_t phase, uint32_t object, uint32_t caller,
                            uint32_t variant, uint32_t slot)
{
    static __thread unsigned logged;
    const uint8_t *base = (const uint8_t *)(uintptr_t)g_xbox_mem_offset;
    uint32_t mode, mask_normal, mask_alt;
    if (!amped_probe_getenv("AMPED_CAMERA_TRACE") || caller != 0x00020E61u || logged >= 128) return;
    if (!((object >= 0x10000u && object <= 0x03FFFF93u) ||
          (object >= 0x80000000u && object <= 0x83FFFF93u))) {
        (fprintf)(stderr, "[AMPED_CAMERA_V23] phase=%s caller=%08X invalid_object=%08X\n",
                  phase ? "exit" : "entry", caller, object);
        ++logged;
        return;
    }
    memcpy(&mode, base + object + 0x50u, sizeof mode);
    memcpy(&mask_normal, base + object + 0x54u, sizeof mask_normal);
    memcpy(&mask_alt, base + object + 0x58u, sizeof mask_alt);
    (fprintf)(stderr, "[AMPED_CAMERA_V23] phase=%s object=%08X caller=%08X variant=%u slot=%u mode=%u normal_mask=%08X alt_mask=%08X flag=%u\n",
              phase ? "exit" : "entry", object, caller, variant, slot, mode,
              mask_normal, mask_alt, base[object + 0x6Cu]);
    if (++logged == 128) (fprintf)(stderr, "[AMPED_CAMERA_V23] per-thread cap reached\n");
}

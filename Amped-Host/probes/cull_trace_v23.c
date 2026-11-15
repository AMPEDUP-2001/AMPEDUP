/* Bounded, read-only observation of the tracked boarder's visibility test. */
#include "probe_env_v23.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ptrdiff_t g_xbox_mem_offset;
extern volatile uint32_t g_xbox_flip_count;
extern int amped_is_boarder_predicate_v23(uint32_t predicate);

static int read_guest(uint32_t va, void *out, uint32_t size)
{
    uint64_t end = (uint64_t)va + size;
    if (!g_xbox_mem_offset || !size ||
        !((va >= 0x10000u && end <= 0x04000000ull) ||
          (va >= 0x80000000u && end <= 0x84000000ull))) return 0;
    memcpy(out, (const void *)((uintptr_t)g_xbox_mem_offset + va), size);
    return 1;
}

/* 0=plane reject, 1=all planes pass, 2=plane result, 3=distance
 * result, 4=fallback result. Only phase 0 supplies a plane address. */
void amped_trace_cull_v23(uint32_t phase, uint32_t predicate, uint32_t camera,
                          uint32_t data, uint32_t plane_y)
{
    static unsigned counts[5];
    uint32_t flip = g_xbox_flip_count;
    const char *minimum;
    float sphere[4] = {0}, center[3] = {0}, plane[4] = {0};
    int sphere_ok = 0, center_ok = 0, plane_ok = 0;
    unsigned ordinal;
    if (!amped_probe_getenv("AMPED_CULL_TRACE") || phase >= 5 ||
        !amped_is_boarder_predicate_v23(predicate)) return;
    minimum = amped_probe_getenv("AMPED_BOARDER_NODE_FROM");
    if (minimum && *minimum && flip < strtoul(minimum, NULL, 10)) return;
#if defined(AMPED_PROJECTION_CORRELATE_V23)
    if (phase == 1) {
        extern void amped_projection_camera_snapshot_v23(uint32_t, uint32_t);
        amped_projection_camera_snapshot_v23(camera, flip);
    }
#endif
    ordinal = __atomic_fetch_add(&counts[phase], 1, __ATOMIC_RELAXED);
    if (ordinal >= 32) return;
    if (phase == 0) {
        if (predicate <= UINT32_MAX - 4u) sphere_ok = read_guest(predicate + 4u, sphere, sizeof sphere);
        center_ok = read_guest(0x0019E1C8u, center, sizeof center);
        if (plane_y >= 4u) plane_ok = read_guest(plane_y - 4u, plane, sizeof plane);
    }
    (fprintf)(stderr, "[AMPED_CULL_V23] phase=%u flip=%u predicate=%08X camera=%08X data=%u plane_y=%08X sphere_ok=%d sphere=%g,%g,%g,%g center_ok=%d center=%g,%g,%g plane_ok=%d plane=%g,%g,%g,%g\n",
              phase, flip, predicate, camera, data, plane_y, sphere_ok,
              sphere[0], sphere[1], sphere[2], sphere[3], center_ok,
              center[0], center[1], center[2], plane_ok,
              plane[0], plane[1], plane[2], plane[3]);
}

void amped_trace_distance_v23(uint32_t predicate, uint32_t camera, uint32_t center_va,
                              double threshold_fp, double distance_fp)
{
    static unsigned count;
    const char *minimum = amped_probe_getenv("AMPED_BOARDER_NODE_FROM");
    uint32_t flip = g_xbox_flip_count;
    float center[3] = {0}, camera_xyz[3] = {0}, far_clip = 0, radius = 0;
    int center_ok, camera_ok = 0, far_ok = 0, radius_ok = 0;
    if (!amped_probe_getenv("AMPED_CULL_TRACE") || !amped_is_boarder_predicate_v23(predicate) ||
        (minimum && *minimum && flip < strtoul(minimum, NULL, 10))) return;
    if (__atomic_fetch_add(&count, 1, __ATOMIC_RELAXED) >= 32) return;
    center_ok = read_guest(center_va, center, sizeof center);
    if (camera <= UINT32_MAX - 0x180u) camera_ok = read_guest(camera + 0x180u, camera_xyz, sizeof camera_xyz);
    if (camera <= UINT32_MAX - 0xE4u) far_ok = read_guest(camera + 0xE4u, &far_clip, sizeof far_clip);
    if (predicate <= UINT32_MAX - 0x10u) radius_ok = read_guest(predicate + 0x10u, &radius, sizeof radius);
    (fprintf)(stderr, "[AMPED_CULL_DISTANCE] flip=%u predicate=%08X camera=%08X center_ok=%d center=%g,%g,%g camera_ok=%d camera_xyz=%g,%g,%g far_ok=%d far=%g radius_ok=%d radius=%g fp_threshold=%.17g fp_distance=%.17g\n",
              flip, predicate, camera, center_ok, center[0], center[1], center[2],
              camera_ok, camera_xyz[0], camera_xyz[1], camera_xyz[2], far_ok, far_clip,
              radius_ok, radius, threshold_fp, distance_fp);
}

void amped_trace_fallback_v23(uint32_t phase, uint32_t predicate, uint32_t status, uint32_t output)
{
    static unsigned counts[2];
    const char *minimum = amped_probe_getenv("AMPED_BOARDER_NODE_FROM");
    uint32_t flip = g_xbox_flip_count, object = 0, vt = 0, method = 0;
    uint32_t field24 = 0, field28 = 0, field2c = 0, field30 = 0;
    uint32_t device = 0, block = 0, record = 0, report[4] = {0};
    uint64_t block_address, record_address;
    uint8_t enabled = 0;
    int object_ok, vt_ok = 0, method_ok = 0, enabled_ok = 0;
    int f24_ok = 0, f28_ok = 0, f2c_ok = 0, f30_ok = 0;
    int device_ok = 0, block_ok = 0, record_ok = 0, report_ok = 0;
    if (!amped_probe_getenv("AMPED_CULL_TRACE") || phase > 1 || !amped_is_boarder_predicate_v23(predicate) ||
        (minimum && *minimum && flip < strtoul(minimum, NULL, 10))) return;
    if (__atomic_fetch_add(&counts[phase], 1, __ATOMIC_RELAXED) >= 32) return;
    object_ok = predicate <= UINT32_MAX - 0x2Cu && read_guest(predicate + 0x2Cu, &object, 4);
    if (object_ok && object) {
        vt_ok = read_guest(object, &vt, 4);
        if (vt_ok && vt <= UINT32_MAX - 4u) method_ok = read_guest(vt + 4u, &method, 4);
        if (object <= UINT32_MAX - 0x24u) f24_ok = read_guest(object + 0x24u, &field24, 4);
        if (object <= UINT32_MAX - 0x28u) f28_ok = read_guest(object + 0x28u, &field28, 4);
        if (object <= UINT32_MAX - 0x2Cu) f2c_ok = read_guest(object + 0x2Cu, &field2c, 4);
        if (object <= UINT32_MAX - 0x30u) f30_ok = read_guest(object + 0x30u, &field30, 4);
        if (object <= UINT32_MAX - 0x31u) enabled_ok = read_guest(object + 0x31u, &enabled, 1);
    }
    /* The handle maps to a 16-byte report slot in a per-device block. This
     * snapshot may race a guest/GPU update; log validity instead of assuming
     * allocation or ownership. The low-24 address is only a correlation key. */
    if (f24_ok) {
        device_ok = read_guest(0x0010C970u, &device, sizeof device) && device != 0;
        block_address = (uint64_t)device + 0x48Cu + (uint64_t)(field24 >> 8) * 4u;
        if (device_ok && block_address <= UINT32_MAX)
            block_ok = read_guest((uint32_t)block_address, &block, sizeof block) && block != 0;
        record_address = (uint64_t)block + 16u * (field24 & 0xFFu);
        if (block_ok && record_address <= UINT32_MAX) {
            record = (uint32_t)record_address;
            record_ok = 1;
            report_ok = read_guest(record, report, sizeof report);
        }
    }
    (fprintf)(stderr, "[AMPED_CULL_FALLBACK] flip=%u phase=%u predicate=%08X object_ok=%d object=%08X vt_ok=%d vt=%08X method_ok=%d method=%08X f24_ok=%d f24=%08X f28_ok=%d f28=%08X f2c_ok=%d f2c=%08X f30_ok=%d f30=%08X byte31_ok=%d byte31=%u status=%08X output=%u\n",
              flip, phase, predicate, object_ok, object, vt_ok, vt, method_ok, method,
              f24_ok, field24, f28_ok, field28, f2c_ok, field2c, f30_ok, field30,
              enabled_ok, enabled, status, output);
    (fprintf)(stderr, "[AMPED_CULL_REPORT] flip=%u phase=%u handle_ok=%d handle=%08X device_ok=%d device=%08X block_ok=%d block=%08X record_ok=%d record=%08X low24=%06X report_ok=%d timestamp_lo=%08X timestamp_hi=%08X count=%u status=%08X\n",
              flip, phase, f24_ok, field24, device_ok, device, block_ok, block,
              record_ok, record, record & 0x00FFFFFFu, report_ok,
              report[0], report[1], report[2], report[3]);
}

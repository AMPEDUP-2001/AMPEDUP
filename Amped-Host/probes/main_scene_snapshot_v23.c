/* Opt-in one-time guest scene-root/parent snapshot. No guest state is changed.
 * It can race scene updates and is not proof a node was rendered. */
#include "probe_env_v23.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ptrdiff_t g_xbox_mem_offset;
extern volatile uint32_t g_xbox_flip_count;

static int read_guest_u32(uint32_t va, uint32_t *out)
{
    const uint8_t *base = (const uint8_t *)(uintptr_t)g_xbox_mem_offset;
    if (!base || !((va >= 0x00010000u && va <= 0x03FFFFFCu) ||
                   (va >= 0x80000000u && va <= 0x83FFFFFCu))) return 0;
    memcpy(out, base + va, sizeof *out);
    return 1;
}

void amped_scene_snapshot_v23(uint32_t frame, uint32_t owner)
{
    static unsigned done;
    static const uint32_t root_offsets[] = {0x18, 0x1C, 0x2C, 0x3C, 0x4C, 0x5C, 0x60, 0x2A84};
    const char *flag = amped_probe_getenv("AMPED_SCENE_SNAPSHOT"), *minimum;
    uint32_t engine, mode = UINT32_MAX, skip = UINT32_MAX, roots[8] = {0}, current = frame, seen[32], owner_index_word = UINT32_MAX;
    unsigned i, depth, seen_count = 0, flip = g_xbox_flip_count;
    if (!flag || strcmp(flag, "1")) return;
    minimum = amped_probe_getenv("AMPED_BOARDER_NODE_FROM");
    if (minimum && *minimum && flip < strtoul(minimum, NULL, 10)) return;
    if (__atomic_exchange_n(&done, 1, __ATOMIC_ACQ_REL)) return;
    if (owner <= UINT32_MAX - 0x19Cu) read_guest_u32(owner + 0x19Cu, &owner_index_word);
    (fprintf)(stderr, "[AMPED_SCENE] flip=%u owner=%08X owner_index_byte=%02X frame=%08X (index semantics unverified)\n",
              flip, owner, (owner_index_word >> 8) & 0xFFu, frame);
    if (!read_guest_u32(0x0018F7C8u, &engine) || engine > UINT32_MAX - 0x2A98u ||
        !read_guest_u32(engine + 0x2A94u, &skip) || !read_guest_u32(engine + 0x2A74u, &mode)) {
        (fprintf)(stderr, "[AMPED_SCENE] flip=%u frame=%08X invalid engine\n", flip, frame);
        return;
    }
    for (i = 0; i < 8; ++i) {
        int valid = read_guest_u32(engine + root_offsets[i], &roots[i]);
        uint32_t active = UINT32_MAX;
        if (i >= 1 && i <= 4) read_guest_u32(engine + root_offsets[i] + 0x0Cu, &active);
        (fprintf)(stderr, "[AMPED_SCENE] flip=%u frame=%08X engine=%08X guest_mode=%08X skip_main=%08X root_offset=%04X root=%08X valid=%d slot_active_word=%08X\n",
                  flip, frame, engine, mode, skip, root_offsets[i], roots[i], valid, active);
    }
    for (depth = 0; depth < 32 && current; ++depth) {
        uint32_t vt, parent, flags, mask = 0;
        if (current > UINT32_MAX - 0x10u || !read_guest_u32(current, &vt) ||
            !read_guest_u32(current + 0x0Cu, &parent) || !read_guest_u32(current + 0x10u, &flags)) {
            (fprintf)(stderr, "[AMPED_SCENE] depth=%u invalid_node=%08X\n", depth, current);
            return;
        }
        for (i = 0; i < seen_count; ++i)
            if (seen[i] == current) { (fprintf)(stderr, "[AMPED_SCENE] cycle=%08X depth=%u\n", current, depth); return; }
        seen[seen_count++] = current;
        for (i = 0; i < 8; ++i) if (roots[i] && roots[i] == current) mask |= 1u << i;
        (fprintf)(stderr, "[AMPED_SCENE] flip=%u depth=%u node=%08X vtable=%08X flags=%08X parent=%08X root_match_mask=%02X\n",
                  flip, depth, current, vt, flags, parent, mask);
        current = parent;
    }
    if (current) (fprintf)(stderr, "[AMPED_SCENE] parent depth cap reached at %08X\n", current);
}

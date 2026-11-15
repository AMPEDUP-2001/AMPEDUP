/* Optional bounded snapshots of game-created PlayerBoarder candidates.
 * No guest state is changed; these fields alone do not prove active player. */
#include "probe_env_v23.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ptrdiff_t g_xbox_mem_offset;
extern volatile uint32_t g_xbox_flip_count;

enum { BOARDER_TRACKED = 8, BOARDER_NODE_CAP = 256, BOARDER_LEAF_CAP = 64, BOARDER_WALK_CAP = 512 };
static struct { uint32_t owner, frame, color, model, walk_attempts, walked; } s_boarders[BOARDER_TRACKED];
static struct { uint32_t owner, node; } s_boarder_leaves[BOARDER_LEAF_CAP];
static unsigned s_boarder_leaf_count, s_boarder_walk_lock;

static int guest_word(uint32_t guest, uint32_t *out)
{
    const uint8_t *base = (const uint8_t *)(uintptr_t)g_xbox_mem_offset;
    if (!base || !((guest >= 0x00010000u && guest <= 0x03FFFFFCu) ||
                   (guest >= 0x80000000u && guest <= 0x83FFFFFCu))) return 0;
    memcpy(out, base + guest, sizeof *out);
    return 1;
}

/* Re-discover the currently registered boarder's predicate, not an address
 * recorded in another run. The capture is a guarded asynchronous snapshot. */
int amped_is_boarder_predicate_v23(uint32_t predicate)
{
    uint32_t engine, root, candidate;
    unsigned i, j;
    if (!predicate || !guest_word(0x0018F7C8u, &engine) || engine > UINT32_MAX - 0x5Cu)
        return 0;
    for (i = 0; i < 4; ++i) {
        uint32_t off = 0x1Cu + 16u * i;
        if (!guest_word(engine + off, &root) || !root ||
            !guest_word(engine + off + 8u, &candidate) || candidate != predicate) continue;
        for (j = 0; j < BOARDER_TRACKED; ++j)
            if (__atomic_load_n(&s_boarders[j].owner, __ATOMIC_ACQUIRE) &&
                root == __atomic_load_n(&s_boarders[j].frame, __ATOMIC_ACQUIRE)) return 1;
    }
    return 0;
}

/* Aggregate tracked observations by guest mode, not by inferred GPU pass.
 * Flips and guest state may change asynchronously during sampling. */
enum { BOARDER_AGG_STAGES = 13, BOARDER_AGG_MODES = 4, BOARDER_AGG_CALLERS = 8 };
static struct {
    unsigned lock, dropped;
    int initialized, done;
    uint32_t first, last, frames;
    uint64_t counts[BOARDER_AGG_MODES][BOARDER_AGG_STAGES];
    struct { uint32_t caller; uint64_t counts[BOARDER_AGG_MODES]; } callers[BOARDER_AGG_CALLERS];
    uint32_t callers_used, callers_overflow;
} s_boarder_agg;

static void boarder_aggregate(uint32_t stage, uint32_t caller, uint32_t flip)
{
    uint32_t engine = 0, mode = UINT32_MAX, bucket, i;
    const char *flag = amped_probe_getenv("AMPED_BOARDER_AGGREGATE");
    if (!flag || strcmp(flag, "1") || stage >= BOARDER_AGG_STAGES) return;
    if (__atomic_exchange_n(&s_boarder_agg.lock, 1, __ATOMIC_ACQUIRE)) {
        __atomic_fetch_add(&s_boarder_agg.dropped, 1, __ATOMIC_RELAXED); return;
    }
    if (s_boarder_agg.done) goto done;
    if (!s_boarder_agg.initialized) {
        const char *value = amped_probe_getenv("AMPED_BOARDER_AGG_FRAMES");
        char *end = NULL;
        unsigned long frames = value ? strtoul(value, &end, 10) : 120;
        if (value && (!*value || !end || *end || !frames || frames > 1000)) frames = 120;
        s_boarder_agg.frames = (uint32_t)frames;
        s_boarder_agg.first = flip;
        s_boarder_agg.last = flip > UINT32_MAX - (uint32_t)frames + 1u ? UINT32_MAX : flip + (uint32_t)frames - 1u;
        s_boarder_agg.initialized = 1;
        (fprintf)(stderr, "[AMPED_BOARDER_AGG] start=%u last=%u frames=%u (guest-mode snapshots; async timing)\n",
                  s_boarder_agg.first, s_boarder_agg.last, s_boarder_agg.frames);
    }
    if (flip > s_boarder_agg.last) {
        static const char *const names[BOARDER_AGG_MODES] = {"zero", "one", "other", "unknown"};
        for (bucket = 0; bucket < BOARDER_AGG_MODES; ++bucket) {
            const uint64_t *c = s_boarder_agg.counts[bucket];
            (fprintf)(stderr, "[AMPED_BOARDER_AGG] mode=%s frame=%llu color=%llu model=%llu model_pred=%llu leaf=%llu leaf_pred=%llu leaf_dispatch=%llu draw_entry=%llu branch_zero=%llu branch_nonzero=%llu fd_zero=%llu fd_nonzero=%llu error=%llu\n",
                      names[bucket], (unsigned long long)c[0], (unsigned long long)c[1],
                      (unsigned long long)c[2], (unsigned long long)c[3], (unsigned long long)c[4],
                      (unsigned long long)c[5], (unsigned long long)c[6], (unsigned long long)c[7],
                      (unsigned long long)c[8], (unsigned long long)c[9], (unsigned long long)c[10],
                      (unsigned long long)c[11], (unsigned long long)c[12]);
        }
        for (i = 0; i < s_boarder_agg.callers_used; ++i)
            (fprintf)(stderr, "[AMPED_BOARDER_AGG] frame_caller=%08X zero=%llu one=%llu other=%llu unknown=%llu\n",
                      s_boarder_agg.callers[i].caller,
                      (unsigned long long)s_boarder_agg.callers[i].counts[0],
                      (unsigned long long)s_boarder_agg.callers[i].counts[1],
                      (unsigned long long)s_boarder_agg.callers[i].counts[2],
                      (unsigned long long)s_boarder_agg.callers[i].counts[3]);
        (fprintf)(stderr, "[AMPED_BOARDER_AGG] done observed_through=%u dropped_lock=%u extra_frame_callers=%u (runtime snapshots, not pass identification)\n",
                  flip, __atomic_load_n(&s_boarder_agg.dropped, __ATOMIC_RELAXED), s_boarder_agg.callers_overflow);
        s_boarder_agg.done = 1;
        goto done;
    }
    if (flip < s_boarder_agg.first) goto done;
    if (guest_word(0x0018F7C8u, &engine) && engine <= UINT32_MAX - 0x2A74u)
        guest_word(engine + 0x2A74u, &mode);
    bucket = mode == 0 ? 0 : mode == 1 ? 1 : mode == UINT32_MAX ? 3 : 2;
    ++s_boarder_agg.counts[bucket][stage];
    if (stage == 0) {
        for (i = 0; i < s_boarder_agg.callers_used; ++i)
            if (s_boarder_agg.callers[i].caller == caller) break;
        if (i == s_boarder_agg.callers_used && i < BOARDER_AGG_CALLERS) {
            s_boarder_agg.callers[i].caller = caller;
            ++s_boarder_agg.callers_used;
        }
        if (i < BOARDER_AGG_CALLERS) ++s_boarder_agg.callers[i].counts[bucket];
        else ++s_boarder_agg.callers_overflow;
    }
done:
    __atomic_store_n(&s_boarder_agg.lock, 0, __ATOMIC_RELEASE);
}

/* Capture a bounded snapshot of a model's descendants. Guest memory may be
 * changing on another thread: a malformed link stops that branch; it never
 * changes the graph. Skip the root's own siblings. */
static void discover_boarder_leaves(unsigned boarder, uint32_t owner, uint32_t model)
{
    uint32_t queue[BOARDER_WALK_CAP], seen[BOARDER_WALK_CAP];
    unsigned head = 0, tail = 0, visited = 0, found = 0, invalid = 0, capped = 0;
    uint32_t first;
    if (__atomic_load_n(&s_boarders[boarder].walked, __ATOMIC_ACQUIRE) ||
        __atomic_exchange_n(&s_boarder_walk_lock, 1, __ATOMIC_ACQUIRE)) return;
    if (++s_boarders[boarder].walk_attempts > 8) goto done;
    if (model > UINT32_MAX - 4u || !guest_word(model + 4u, &first)) { ++invalid; goto report; }
    if (first) queue[tail++] = first;
    while (head < tail && visited < BOARDER_WALK_CAP) {
        uint32_t node = queue[head++], vt, child, next;
        unsigned i;
        for (i = 0; i < visited; ++i) if (seen[i] == node) break;
        if (i < visited) continue;
        seen[visited++] = node;
        if (node > UINT32_MAX - 0x14u || !guest_word(node, &vt) ||
            !guest_word(node + 4u, &child) || !guest_word(node + 0x14u, &next)) {
            ++invalid; continue;
        }
        if (vt == 0x00178B58u) {
            unsigned n = __atomic_load_n(&s_boarder_leaf_count, __ATOMIC_RELAXED);
            for (i = 0; i < n; ++i)
                if (__atomic_load_n(&s_boarder_leaves[i].node, __ATOMIC_ACQUIRE) == node) break;
            if (i == n && n < BOARDER_LEAF_CAP) {
                __atomic_store_n(&s_boarder_leaves[n].owner, owner, __ATOMIC_RELAXED);
                __atomic_store_n(&s_boarder_leaves[n].node, node, __ATOMIC_RELEASE);
                __atomic_store_n(&s_boarder_leaf_count, n + 1, __ATOMIC_RELEASE);
            } else if (i == n) ++capped;
            ++found;
        }
        if (child) { if (tail < BOARDER_WALK_CAP) queue[tail++] = child; else ++capped; }
        if (next) { if (tail < BOARDER_WALK_CAP) queue[tail++] = next; else ++capped; }
    }
    if (head < tail) ++capped;
report:
    (fprintf)(stderr, "[AMPED_BOARDER_GRAPH] flip=%u owner=%08X model=%08X visited=%u vbgeom=%u invalid=%u capped=%u attempt=%u (snapshot; no cross-frame guarantee)\n",
              g_xbox_flip_count, owner, model, visited, found, invalid, capped, s_boarders[boarder].walk_attempts);
    if (found || s_boarders[boarder].walk_attempts >= 8)
        __atomic_store_n(&s_boarders[boarder].walked, 1, __ATOMIC_RELEASE);
done:
    __atomic_store_n(&s_boarder_walk_lock, 0, __ATOMIC_RELEASE);
}

void amped_trace_boarder_v23(uint32_t phase, uint32_t owner, uint32_t manager,
                             uint32_t slot, uint32_t site)
{
    static unsigned count;
    uint32_t vtable, model, frame, color, model_vt = 0, frame_vt = 0, color_vt = 0;
    unsigned ordinal;
    if (!amped_probe_getenv("AMPED_BOARDER_TRACE")) return;
    ordinal = __atomic_fetch_add(&count, 1, __ATOMIC_RELAXED);
    if (ordinal >= 128) return;
    if (owner > UINT32_MAX - 0x0Cu || !guest_word(owner, &vtable) ||
        !guest_word(owner + 4, &model) || !guest_word(owner + 8, &frame) ||
        !guest_word(owner + 0x0C, &color)) {
        (fprintf)(stderr, "[AMPED_BOARDER] phase=%s site=%08X invalid_owner=%08X flip=%u\n",
                  phase ? "setup" : "slot", site, owner, g_xbox_flip_count);
        return;
    }
    guest_word(model, &model_vt); guest_word(frame, &frame_vt); guest_word(color, &color_vt);
    if (phase && vtable == 0x00174E18u) {
        unsigned i;
        for (i = 0; i < BOARDER_TRACKED; ++i) {
            uint32_t observed = __atomic_load_n(&s_boarders[i].owner, __ATOMIC_ACQUIRE);
            if (observed == owner || (!observed &&
                __atomic_compare_exchange_n(&s_boarders[i].owner, &observed, owner, 0,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))) {
                __atomic_store_n(&s_boarders[i].frame, frame, __ATOMIC_RELEASE);
                __atomic_store_n(&s_boarders[i].color, color, __ATOMIC_RELEASE);
                __atomic_store_n(&s_boarders[i].model, model, __ATOMIC_RELEASE);
                break;
            }
        }
        if (i == BOARDER_TRACKED)
            (fprintf)(stderr, "[AMPED_BOARDER] owner registry full; additional owner=%08X not tracked\n", owner);
    }
    (fprintf)(stderr, "[AMPED_BOARDER] phase=%s site=%08X flip=%u manager=%08X slot=%u owner=%08X owner_vt=%08X model=%08X model_vt=%08X frame=%08X frame_vt=%08X color=%08X color_vt=%08X player_type=%d\n",
              phase ? "setup" : "slot", site, g_xbox_flip_count, manager, slot, owner,
              vtable, model, model_vt, frame, frame_vt, color, color_vt, vtable == 0x00174E18u);
}

/* kind=0 frame, 1 color, 2 model entry, 3 model predicate result.
 * `caller` is the guest return address and `argument` is the traversal
 * argument; neither by itself identifies a rendering pass. */
void amped_trace_boarder_node_v23(uint32_t kind, uint32_t node, uint32_t caller,
                                  uint32_t argument, uint32_t decision)
{
    static unsigned count;
    const char *minimum;
    uint32_t flip, flags = 0, child = 0, auxiliary = 0, owner = 0;
    unsigned i, ordinal;
    if (!amped_probe_getenv("AMPED_BOARDER_TRACE") || kind > 3 || !node) return;
    flip = g_xbox_flip_count;
    minimum = amped_probe_getenv("AMPED_BOARDER_NODE_FROM");
    if (minimum && *minimum && flip < strtoul(minimum, NULL, 10)) return;
    for (i = 0; i < BOARDER_TRACKED; ++i) {
        uint32_t candidate = kind == 0 ? __atomic_load_n(&s_boarders[i].frame, __ATOMIC_ACQUIRE) :
                             kind == 1 ? __atomic_load_n(&s_boarders[i].color, __ATOMIC_ACQUIRE) :
                                         __atomic_load_n(&s_boarders[i].model, __ATOMIC_ACQUIRE);
        if (node == candidate) { owner = __atomic_load_n(&s_boarders[i].owner, __ATOMIC_ACQUIRE); break; }
    }
    if (!owner) return;
    if (kind == 0) {
        extern void amped_scene_snapshot_v23(uint32_t, uint32_t);
        amped_scene_snapshot_v23(node, owner);
    }
    if (kind == 2) discover_boarder_leaves(i, owner, node);
    boarder_aggregate(kind, caller, flip);
    ordinal = __atomic_fetch_add(&count, 1, __ATOMIC_RELAXED);
    if (ordinal >= BOARDER_NODE_CAP) return;
    if (node > UINT32_MAX - 0x20u || !guest_word(node + 0x10u, &flags) ||
        !guest_word(node + 4u, &child) || !guest_word(node + 0x20u, &auxiliary)) {
        (fprintf)(stderr, "[AMPED_BOARDER_NODE] kind=%u flip=%u owner=%08X invalid_node=%08X\n", kind, flip, owner, node);
        return;
    }
    (fprintf)(stderr, "[AMPED_BOARDER_NODE] kind=%u flip=%u owner=%08X node=%08X caller=%08X arg=%08X flags=%08X child=%08X aux=%08X decision=%u\n",
              kind, flip, owner, node, caller, argument, flags, child, auxiliary, decision);
}

/* Observe +0x1c traversal; node lookup is limited to tracked boarder nodes.
 * Kinds 0/2 are entries, 1/3 are returns from the node's +0x28 method. */
void amped_trace_boarder_main_v23(uint32_t kind, uint32_t node,
                                  uint32_t caller, uint32_t status)
{
    static unsigned counts[4][4];
    uint32_t owner = 0, node_type = UINT32_MAX, engine = 0, mode = UINT32_MAX;
    uint32_t flip = g_xbox_flip_count, flags = 0;
    const char *minimum;
    unsigned i, n, ordinal;
    if (!amped_probe_getenv("AMPED_BOARDER_TRACE") || !node || kind > 3) return;
    minimum = amped_probe_getenv("AMPED_BOARDER_NODE_FROM");
    if (minimum && *minimum && flip < strtoul(minimum, NULL, 10)) return;
    for (i = 0; i < BOARDER_TRACKED; ++i) {
        uint32_t o = __atomic_load_n(&s_boarders[i].owner, __ATOMIC_ACQUIRE);
        if (!o) continue;
        if (node == __atomic_load_n(&s_boarders[i].frame, __ATOMIC_ACQUIRE)) node_type = 0;
        else if (node == __atomic_load_n(&s_boarders[i].color, __ATOMIC_ACQUIRE)) node_type = 1;
        else if (node == __atomic_load_n(&s_boarders[i].model, __ATOMIC_ACQUIRE)) node_type = 2;
        else continue;
        owner = o;
        break;
    }
    if (!owner) {
        n = __atomic_load_n(&s_boarder_leaf_count, __ATOMIC_ACQUIRE);
        for (i = 0; i < n; ++i)
            if (__atomic_load_n(&s_boarder_leaves[i].node, __ATOMIC_ACQUIRE) == node) {
                owner = __atomic_load_n(&s_boarder_leaves[i].owner, __ATOMIC_RELAXED);
                node_type = 3;
                break;
            }
    }
    if (!owner) return;
    ordinal = __atomic_fetch_add(&counts[node_type][kind], 1, __ATOMIC_RELAXED);
    if (ordinal >= 16) return;
    if (node <= UINT32_MAX - 0x10u) guest_word(node + 0x10u, &flags);
    if (guest_word(0x0018F7C8u, &engine) && engine <= UINT32_MAX - 0x2A74u)
        guest_word(engine + 0x2A74u, &mode);
    (fprintf)(stderr, "[AMPED_BOARDER_MAIN] kind=%u type=%u flip=%u owner=%08X node=%08X caller=%08X flags=%08X guest_mode=%08X status=%08X\n",
              kind, node_type, flip, owner, node, caller, flags, mode, status);
}

/* Leaf kinds: 0 traversal entry, 1 visibility predicate return, 2 draw-like
 * dispatch entry. This observes only descendants discovered in the snapshot. */
void amped_trace_boarder_leaf_v23(uint32_t kind, uint32_t node, uint32_t caller,
                                  uint32_t argument, uint32_t decision)
{
    static unsigned count;
    const char *minimum;
    uint32_t flip, owner = 0, flags = 0, resource = 0, predicate = 0;
    unsigned i, n, ordinal;
    if (!amped_probe_getenv("AMPED_BOARDER_TRACE") || !node || kind > 2) return;
    flip = g_xbox_flip_count;
    minimum = amped_probe_getenv("AMPED_BOARDER_NODE_FROM");
    if (minimum && *minimum && flip < strtoul(minimum, NULL, 10)) return;
    n = __atomic_load_n(&s_boarder_leaf_count, __ATOMIC_ACQUIRE);
    for (i = 0; i < n; ++i)
        if (__atomic_load_n(&s_boarder_leaves[i].node, __ATOMIC_ACQUIRE) == node) {
            owner = __atomic_load_n(&s_boarder_leaves[i].owner, __ATOMIC_RELAXED); break;
        }
    if (!owner) return;
    boarder_aggregate(4u + kind, caller, flip);
    ordinal = __atomic_fetch_add(&count, 1, __ATOMIC_RELAXED);
    if (ordinal >= BOARDER_NODE_CAP) return;
    if (node > UINT32_MAX - 0x24u || !guest_word(node + 0x10u, &flags) ||
        !guest_word(node + 0x24u, &resource)) {
        (fprintf)(stderr, "[AMPED_BOARDER_LEAF] kind=%u flip=%u owner=%08X invalid_node=%08X\n", kind, flip, owner, node);
        return;
    }
    if (resource <= UINT32_MAX - 0x30u) guest_word(resource + 0x30u, &predicate);
    (fprintf)(stderr, "[AMPED_BOARDER_LEAF] kind=%u flip=%u owner=%08X node=%08X caller=%08X arg=%08X flags=%08X resource=%08X predicate=%08X decision=%u\n",
              kind, flip, owner, node, caller, argument, flags, resource, predicate, decision);
}

/* A2910 observation: stage 0 entry, 1/2 branch, 3/4 FD590 call, 5 negative
 * return. Mode is a guest renderer field, not a verified pass identity. */
void amped_trace_boarder_draw_v23(uint32_t stage, uint32_t node, uint32_t site, uint32_t data)
{
    static unsigned count;
    const char *minimum;
    uint32_t flip, owner = 0, resource = 0, parts = UINT32_MAX;
    uint32_t engine = 0, mode = UINT32_MAX;
    uint32_t primitive = UINT32_MAX, index_count = UINT32_MAX, indices = 0;
    int stack_valid = 0;
    unsigned i, n, ordinal;
    if (!amped_probe_getenv("AMPED_BOARDER_TRACE") || !node || stage > 5) return;
    flip = g_xbox_flip_count;
    minimum = amped_probe_getenv("AMPED_BOARDER_NODE_FROM");
    if (minimum && *minimum && flip < strtoul(minimum, NULL, 10)) return;
    n = __atomic_load_n(&s_boarder_leaf_count, __ATOMIC_ACQUIRE);
    for (i = 0; i < n; ++i)
        if (__atomic_load_n(&s_boarder_leaves[i].node, __ATOMIC_ACQUIRE) == node) {
            owner = __atomic_load_n(&s_boarder_leaves[i].owner, __ATOMIC_RELAXED); break;
        }
    if (!owner) return;
    boarder_aggregate(7u + stage, site, flip);
    ordinal = __atomic_fetch_add(&count, 1, __ATOMIC_RELAXED);
    if (ordinal >= BOARDER_NODE_CAP) return;
    if (node <= UINT32_MAX - 0x24u && guest_word(node + 0x24u, &resource) &&
        resource <= UINT32_MAX - 0x28u) guest_word(resource + 0x28u, &parts);
    if (guest_word(0x0018F7C8u, &engine) && engine <= UINT32_MAX - 0x2A74u)
        guest_word(engine + 0x2A74u, &mode);
    if ((stage == 3 || stage == 4) && data <= UINT32_MAX - 8u)
        stack_valid = guest_word(data, &primitive) && guest_word(data + 4u, &index_count) &&
                      guest_word(data + 8u, &indices);
    (fprintf)(stderr, "[AMPED_BOARDER_DRAW] stage=%u flip=%u owner=%08X node=%08X site=%08X resource=%08X parts=%u guest_mode=%08X status=%08X args_valid=%d primitive=%08X index_count=%u indices=%08X\n",
              stage, flip, owner, node, site, resource, parts, mode, stage == 5 ? data : 0,
              stack_valid, primitive, index_count, indices);
}

/* The four-slot candidate draw loop: 0 slot check, 1 predicate returned,
 * 2 root call about to dispatch, 3 root returned. Counts are per stage. */
void amped_trace_boarder_slot_v23(uint32_t stage, uint32_t engine, uint32_t slot, uint32_t result)
{
    static unsigned emitted, stage_counts[4];
    const char *minimum;
    uint32_t root = 0, owner = 0, bound = 0, predicate = 0, predicate_vt = 0;
    uint32_t active = UINT32_MAX, selector = UINT32_MAX, camera = 0, mode = UINT32_MAX, root_vt = 0;
    uint32_t root_offset, flip;
    unsigned i, ordinal, count;
    if (!amped_probe_getenv("AMPED_BOARDER_TRACE") || stage > 3 || slot >= 4) return;
    flip = g_xbox_flip_count;
    minimum = amped_probe_getenv("AMPED_BOARDER_NODE_FROM");
    if (minimum && *minimum && flip < strtoul(minimum, NULL, 10)) return;
    if (engine > UINT32_MAX - 0x2A74u) return;
    root_offset = 0x1Cu + slot * 0x10u;
    if (!guest_word(engine + root_offset, &root) || !root) return;
    for (i = 0; i < BOARDER_TRACKED; ++i)
        if (__atomic_load_n(&s_boarders[i].frame, __ATOMIC_ACQUIRE) == root) {
            owner = __atomic_load_n(&s_boarders[i].owner, __ATOMIC_ACQUIRE); break;
        }
    if (!owner) return;
    count = __atomic_add_fetch(&stage_counts[stage], 1, __ATOMIC_RELAXED);
    ordinal = __atomic_fetch_add(&emitted, 1, __ATOMIC_RELAXED);
    if (ordinal >= BOARDER_NODE_CAP) return;
    guest_word(root, &root_vt);
    guest_word(engine + root_offset + 4u, &bound);
    guest_word(engine + root_offset + 8u, &predicate);
    guest_word(engine + root_offset + 12u, &active);
    if (predicate) guest_word(predicate, &predicate_vt);
    guest_word(engine + 0x29ECu, &selector);
    guest_word(engine + ((selector & 0xFFu) ? 0x14u : 0x10u), &camera);
    guest_word(engine + 0x2A74u, &mode);
    (fprintf)(stderr, "[AMPED_BOARDER_SLOT] stage=%u stage_count=%u flip=%u owner=%08X engine=%08X slot=%u root=%08X root_vt=%08X active=%02X bound=%08X predicate=%08X predicate_vt=%08X camera_selector=%02X camera=%08X guest_mode=%08X result=%08X\n",
              stage, count, flip, owner, engine, slot, root, root_vt, active & 0xFFu,
              bound, predicate, predicate_vt, selector & 0xFFu, camera, mode, result);
}

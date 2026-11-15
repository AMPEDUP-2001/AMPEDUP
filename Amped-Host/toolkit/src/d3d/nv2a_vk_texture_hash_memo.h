#ifndef NV2A_VK_TEXTURE_HASH_MEMO_H
#define NV2A_VK_TEXTURE_HASH_MEMO_H

#include <stdlib.h>
#include <string.h>
#include "nv2a_backend_common.h"

#define NVK_TEXTURE_HASH_SLOTS 128u
#define NVK_TEXTURE_HASH_MIN (4u * 1024u)
#define NVK_TEXTURE_HASH_MAX (256u * 1024u)
#define NVK_TEXTURE_HASH_BUDGET (16u * 1024u * 1024u)

typedef struct {
    uint8_t *bytes;
    uint32_t capacity, offset, size;
    uint64_t digest;
} NvkTextureHashEntry;

typedef struct {
    uint32_t capacity;
    NvkTextureHashEntry entries[NVK_TEXTURE_HASH_SLOTS];
} NvkTextureHashMemo;

static inline unsigned nvk_texture_hash_slot(uint32_t offset)
{
    uint32_t hash = (offset ^ (offset >> 16)) * 0x9e3779b1u;
    return (unsigned)(hash >> 25);
}

/* Call after the renderer's usual render-target coherence checks. The digest
 * describes the owned bytes. A hit compares the entire live range. Palette
 * bytes are checked separately by the caller. */
static inline uint64_t nvk_texture_hash_memo(NvkTextureHashMemo *memo,
        const uint8_t *data, uint32_t offset, uint32_t size, int *reused)
{
    NvkTextureHashEntry *entry;
    *reused = 0;
    if (size < NVK_TEXTURE_HASH_MIN || size > NVK_TEXTURE_HASH_MAX)
        return nv2a_tex_hash(data, size, NV2A_TEX_HASH_SEED);
    entry = &memo->entries[nvk_texture_hash_slot(offset)];
    if (entry->size == size && entry->offset == offset
            && !memcmp(entry->bytes, data, size)) {
        *reused = 1;
        return entry->digest;
    }
    if (entry->capacity < size) {
        uint32_t delta = size - entry->capacity;
        if (delta > NVK_TEXTURE_HASH_BUDGET - memo->capacity)
            return nv2a_tex_hash(data, size, NV2A_TEX_HASH_SEED);
        uint8_t *copy = (uint8_t *)realloc(entry->bytes, size);
        if (!copy)
            return nv2a_tex_hash(data, size, NV2A_TEX_HASH_SEED);
        memo->capacity += delta;
        entry->bytes = copy;
        entry->capacity = size;
    }
    memcpy(entry->bytes, data, size);
    entry->digest = nv2a_tex_hash(entry->bytes, size, NV2A_TEX_HASH_SEED);
    entry->offset = offset;
    entry->size = size;
    return entry->digest;
}

static inline void nvk_texture_hash_memo_clear(NvkTextureHashMemo *memo)
{
    for (unsigned i = 0; i < NVK_TEXTURE_HASH_SLOTS; ++i)
        free(memo->entries[i].bytes);
    memset(memo, 0, sizeof *memo);
}

#endif

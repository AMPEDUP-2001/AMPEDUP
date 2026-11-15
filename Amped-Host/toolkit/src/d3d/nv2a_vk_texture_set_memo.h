#ifndef NV2A_VK_TEXTURE_SET_MEMO_H
#define NV2A_VK_TEXTURE_SET_MEMO_H

#include <stdint.h>
#include <string.h>
#include <vulkan/vulkan.h>

#define NVK_TEXTURE_SET_MEMO_SIZE 256

typedef struct {
    uint64_t generation;
    VkDescriptorSet set;
    VkImageView views[4];
    VkSampler samplers[4];
} NvkTextureSetMemoEntry;

typedef struct {
    uint64_t generation;
    NvkTextureSetMemoEntry entries[NVK_TEXTURE_SET_MEMO_SIZE];
} NvkTextureSetMemo;

static inline void nvk_texture_set_memo_reset(NvkTextureSetMemo *memo)
{
    ++memo->generation;
}

static inline unsigned nvk_texture_set_memo_slot(const VkImageView views[4],
                                                  const VkSampler samplers[4])
{
    uint64_t hash = 14695981039346656037ull;
    for (unsigned i = 0; i < 4; ++i) {
        /* Hashes only select a slot. Compare all handles before reuse. */
        hash = (hash ^ (uintptr_t)views[i]) * 1099511628211ull;
        hash = (hash ^ (uintptr_t)samplers[i]) * 1099511628211ull;
    }
    hash ^= (hash >> 13) ^ (hash >> 32);
    return (unsigned)hash & (NVK_TEXTURE_SET_MEMO_SIZE - 1);
}

static inline VkDescriptorSet nvk_texture_set_memo_find(const NvkTextureSetMemo *memo,
        unsigned slot, const VkImageView views[4], const VkSampler samplers[4])
{
    const NvkTextureSetMemoEntry *entry = &memo->entries[slot];
    if (entry->set && entry->generation == memo->generation
            && !memcmp(views, entry->views, sizeof entry->views)
            && !memcmp(samplers, entry->samplers, sizeof entry->samplers))
        return entry->set;
    return VK_NULL_HANDLE;
}

static inline void nvk_texture_set_memo_store(NvkTextureSetMemo *memo, unsigned slot,
        VkDescriptorSet set, const VkImageView views[4], const VkSampler samplers[4])
{
    NvkTextureSetMemoEntry *entry = &memo->entries[slot];
    entry->generation = memo->generation;
    entry->set = set;
    memcpy(entry->views, views, sizeof entry->views);
    memcpy(entry->samplers, samplers, sizeof entry->samplers);
}

#endif

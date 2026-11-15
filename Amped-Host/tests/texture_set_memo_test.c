#include "nv2a_vk_texture_set_memo.h"
#include <stdio.h>

int main(void)
{
    static NvkTextureSetMemo memo;
    VkImageView views[4] = { (VkImageView)(uintptr_t)0x100, (VkImageView)(uintptr_t)0x200 };
    VkSampler samplers[4] = { (VkSampler)(uintptr_t)0x300, (VkSampler)(uintptr_t)0x400 };
    VkDescriptorSet a = (VkDescriptorSet)(uintptr_t)0x500;
    VkDescriptorSet b = (VkDescriptorSet)(uintptr_t)0x600;
    unsigned slot = nvk_texture_set_memo_slot(views, samplers);

    if (nvk_texture_set_memo_find(&memo, slot, views, samplers)) return 1;
    nvk_texture_set_memo_store(&memo, slot, a, views, samplers);
    if (nvk_texture_set_memo_find(&memo, slot, views, samplers) != a) return 2;

    /* Force a bucket collision while changing each image/sampler separately. */
    for (unsigned i = 0; i < 4; ++i) {
        VkImageView view = views[i];
        VkSampler sampler = samplers[i];
        views[i] = (VkImageView)(uintptr_t)0x700;
        if (nvk_texture_set_memo_find(&memo, slot, views, samplers)) return 3;
        views[i] = view;
        samplers[i] = (VkSampler)(uintptr_t)0x800;
        if (nvk_texture_set_memo_find(&memo, slot, views, samplers)) return 4;
        samplers[i] = sampler;
    }

    nvk_texture_set_memo_reset(&memo);
    if (nvk_texture_set_memo_find(&memo, slot, views, samplers)) return 5;
    nvk_texture_set_memo_store(&memo, slot, b, views, samplers);
    if (nvk_texture_set_memo_find(&memo, slot, views, samplers) != b) return 6;
    puts("Texture descriptor memo: full-key checks and generation invalidation passed");
    return 0;
}

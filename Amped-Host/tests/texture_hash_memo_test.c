#include "nv2a_vk_texture_hash_memo.h"
#include <stdio.h>

int main(void)
{
    static NvkTextureHashMemo memo;
    static uint8_t data[NVK_TEXTURE_HASH_MAX + 1];
    const uint32_t size = NVK_TEXTURE_HASH_MIN + 17;
    const uint32_t offset = 0x123400;
    int reused;

    for (uint32_t i = 0; i < sizeof data; ++i)
        data[i] = (uint8_t)(i * 37u + (i >> 8));

    uint64_t expected = nv2a_tex_hash(data, size, NV2A_TEX_HASH_SEED);
    if (nvk_texture_hash_memo(&memo, data, offset, size, &reused) != expected || reused)
        return 1;
    if (nvk_texture_hash_memo(&memo, data, offset, size, &reused) != expected || !reused)
        return 2;
    data[size - 1] ^= 0x80;
    uint64_t changed = nv2a_tex_hash(data, size, NV2A_TEX_HASH_SEED);
    if (changed == expected || nvk_texture_hash_memo(&memo, data, offset, size, &reused) != changed || reused)
        return 3;
    data[size - 1] ^= 0x80;
    if (nvk_texture_hash_memo(&memo, data, offset, size, &reused) != expected || reused)
        return 4;

    uint32_t collision = offset + 1;
    while (nvk_texture_hash_slot(collision) != nvk_texture_hash_slot(offset))
        ++collision;
    if (nvk_texture_hash_memo(&memo, data, collision, size, &reused) != expected || reused)
        return 5;
    if (nvk_texture_hash_memo(&memo, data, offset, size, &reused) != expected || reused)
        return 6;
    expected = nv2a_tex_hash(data, size + 1, NV2A_TEX_HASH_SEED);
    if (nvk_texture_hash_memo(&memo, data, offset, size + 1, &reused) != expected || reused)
        return 7;

    expected = nv2a_tex_hash(data, sizeof data, NV2A_TEX_HASH_SEED);
    if (nvk_texture_hash_memo(&memo, data, offset, sizeof data, &reused) != expected || reused)
        return 8;
    nvk_texture_hash_memo_clear(&memo);
    if (memo.capacity != 0) return 9;

    /* Populate a distinct key in every slot. The memory budget must force
     * some allocations to fall back without changing the hash result. */
    uint8_t seen[NVK_TEXTURE_HASH_SLOTS] = {0};
    unsigned slots = 0, hits = 0;
    expected = nv2a_tex_hash(data, NVK_TEXTURE_HASH_MAX, NV2A_TEX_HASH_SEED);
    for (uint32_t key = 0; key < 100000 && slots < NVK_TEXTURE_HASH_SLOTS; ++key) {
        unsigned slot = nvk_texture_hash_slot(key);
        if (seen[slot]) continue;
        seen[slot] = 1;
        ++slots;
        if (nvk_texture_hash_memo(&memo, data, key, NVK_TEXTURE_HASH_MAX, &reused) != expected || reused)
            return 10;
        if (nvk_texture_hash_memo(&memo, data, key, NVK_TEXTURE_HASH_MAX, &reused) != expected)
            return 11;
        hits += reused;
    }
    if (slots != NVK_TEXTURE_HASH_SLOTS || hits != NVK_TEXTURE_HASH_BUDGET / NVK_TEXTURE_HASH_MAX
            || memo.capacity != NVK_TEXTURE_HASH_BUDGET)
        return 12;
    nvk_texture_hash_memo_clear(&memo);
    puts("Texture hash memo: equality, mutation, collision, length and budget checks passed");
    return 0;
}

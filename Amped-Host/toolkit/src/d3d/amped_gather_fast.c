/* Optional exact-layout Vulkan gather; generic gather remains the fallback. */
#include "amped_gather_fast.h"
#include <stdint.h>
#include <string.h>
int amped_gather_fast(const struct nv2a_pb_draw *d, const int *slot, int n,
                      uint32_t lo, uint32_t count, const uint8_t *mem, float *out)
{
    const VertexAttr *a = d->attr;
    uintptr_t base = (uintptr_t)mem, dst = (uintptr_t)out, start, end, dend;
    uint64_t delta, src_len, dst_len;
    const uint8_t *p;
    uint32_t i;
    if (d->inline_data || n != 3 || slot[0] != 0 || slot[1] != 1 || slot[2] != 2 ||
        !a[0].offset || a[0].offset > UINT32_MAX - 23u ||
        a[0].type != NV2A_VTX_FLOAT || a[0].size != 3 || a[0].stride != 24 ||
        a[1].type != NV2A_VTX_CMP || a[1].size != 1 || a[1].stride != 24 || a[1].offset != a[0].offset + 12 ||
        a[2].type != NV2A_VTX_FLOAT || a[2].size != 2 || a[2].stride != 24 || a[2].offset != a[0].offset + 16)
        return 0;
    delta = (uint64_t)a[0].offset + (uint64_t)lo * 24u;
    src_len = (uint64_t)count * 24u;
    dst_len = (uint64_t)count * 48u;
    if (delta > UINTPTR_MAX - base || src_len > UINTPTR_MAX || dst_len > UINTPTR_MAX) return 0;
    start = base + (uintptr_t)delta;
    if (src_len > UINTPTR_MAX - start || dst_len > UINTPTR_MAX - dst) return 0;
    end = start + (uintptr_t)src_len; dend = dst + (uintptr_t)dst_len;
    if (count && start < dend && dst < end) return 0;
    p = (const uint8_t *)start;
    for (i = 0; i < count; ++i, p += 24, out += 12) {
        float x, y, z, u, v;
        memcpy(&x, p, 4); memcpy(&y, p + 4, 4); memcpy(&z, p + 8, 4);
        out[0] = x; out[1] = y; out[2] = z; out[3] = 1.0f;
        nv2a_vtx_decode(NV2A_VTX_CMP, 1, p + 12, out + 4);
        memcpy(&u, p + 16, 4); memcpy(&v, p + 20, 4);
        out[8] = u; out[9] = v; out[10] = 0.0f; out[11] = 1.0f;
    }
    return 1;
}

/* Exact external slot 0 FLOAT3, slot 2 FLOAT2, slot 3 CMP1, stride 28.
 * This remains an opt-in diagnostic; unsupported layouts write nothing. */
int amped_gather_snow(const struct nv2a_pb_draw *d, const int *slot, int n,
                      uint32_t lo, uint32_t count, const uint8_t *mem, float *out)
{
    const VertexAttr *a = d->attr;
    uintptr_t base = (uintptr_t)mem, dst = (uintptr_t)out, start, end, dend;
    uint64_t delta, src_len, dst_len;
    const uint8_t *p;
    uint32_t i;
    if (d->inline_data || n != 3 || slot[0] != 0 || slot[1] != 2 || slot[2] != 3 ||
        !a[0].offset || a[0].offset > UINT32_MAX - 27u ||
        a[0].type != NV2A_VTX_FLOAT || a[0].size != 3 || a[0].stride != 28 ||
        a[2].type != NV2A_VTX_FLOAT || a[2].size != 2 || a[2].stride != 28 || a[2].offset != a[0].offset + 16 ||
        a[3].type != NV2A_VTX_CMP || a[3].size != 1 || a[3].stride != 28 || a[3].offset != a[0].offset + 24)
        return 0;
    delta = (uint64_t)a[0].offset + (uint64_t)lo * 28u;
    src_len = (uint64_t)count * 28u; dst_len = (uint64_t)count * 48u;
    if (delta > UINTPTR_MAX - base || src_len > UINTPTR_MAX || dst_len > UINTPTR_MAX) return 0;
    start = base + (uintptr_t)delta;
    if (src_len > UINTPTR_MAX - start || dst_len > UINTPTR_MAX - dst) return 0;
    end = start + (uintptr_t)src_len; dend = dst + (uintptr_t)dst_len;
    if (count && start < dend && dst < end) return 0;
    p = (const uint8_t *)start;
    for (i = 0; i < count; ++i, p += 28, out += 12) {
        float x, y, z, u, v;
        memcpy(&x, p, 4); memcpy(&y, p + 4, 4); memcpy(&z, p + 8, 4);
        out[0] = x; out[1] = y; out[2] = z; out[3] = 1.0f;
        memcpy(&u, p + 16, 4); memcpy(&v, p + 20, 4);
        out[4] = u; out[5] = v; out[6] = 0.0f; out[7] = 1.0f;
        nv2a_vtx_decode(NV2A_VTX_CMP, 1, p + 24, out + 8);
    }
    return 1;
}

/* Exact external slots 0 FLOAT3, 1 CMP1, 2 FLOAT2, 3 CMP1, stride 28. */
int amped_gather_snow4(const struct nv2a_pb_draw *d, const int *slot, int n,
                       uint32_t lo, uint32_t count, const uint8_t *mem, float *out)
{
    const VertexAttr *a = d->attr;
    uintptr_t base = (uintptr_t)mem, dst = (uintptr_t)out, start, end, dend;
    uint64_t delta, src_len, dst_len;
    const uint8_t *p; uint32_t i;
    if (d->inline_data || n != 4 || slot[0] != 0 || slot[1] != 1 || slot[2] != 2 || slot[3] != 3 ||
        !a[0].offset || a[0].offset > UINT32_MAX - 27u ||
        a[0].type != NV2A_VTX_FLOAT || a[0].size != 3 || a[0].stride != 28 ||
        a[1].type != NV2A_VTX_CMP || a[1].size != 1 || a[1].stride != 28 || a[1].offset != a[0].offset + 12 ||
        a[2].type != NV2A_VTX_FLOAT || a[2].size != 2 || a[2].stride != 28 || a[2].offset != a[0].offset + 16 ||
        a[3].type != NV2A_VTX_CMP || a[3].size != 1 || a[3].stride != 28 || a[3].offset != a[0].offset + 24) return 0;
    delta = (uint64_t)a[0].offset + (uint64_t)lo * 28u;
    src_len = (uint64_t)count * 28u; dst_len = (uint64_t)count * 64u;
    if (delta > UINTPTR_MAX - base || src_len > UINTPTR_MAX || dst_len > UINTPTR_MAX) return 0;
    start = base + (uintptr_t)delta;
    if (src_len > UINTPTR_MAX - start || dst_len > UINTPTR_MAX - dst) return 0;
    end = start + (uintptr_t)src_len; dend = dst + (uintptr_t)dst_len;
    if (count && start < dend && dst < end) return 0;
    p = (const uint8_t *)start;
    for (i = 0; i < count; ++i, p += 28, out += 16) {
        float x, y, z, u, v;
        memcpy(&x, p, 4); memcpy(&y, p + 4, 4); memcpy(&z, p + 8, 4);
        out[0] = x; out[1] = y; out[2] = z; out[3] = 1.0f;
        nv2a_vtx_decode(NV2A_VTX_CMP, 1, p + 12, out + 4);
        memcpy(&u, p + 16, 4); memcpy(&v, p + 20, 4);
        out[8] = u; out[9] = v; out[10] = 0.0f; out[11] = 1.0f;
        nv2a_vtx_decode(NV2A_VTX_CMP, 1, p + 24, out + 12);
    }
    return 1;
}

/* Exact external slots 0 FLOAT3, 1 FLOAT2, 2 FLOAT2, stride 28. */
int amped_gather_snowuv(const struct nv2a_pb_draw *d, const int *slot, int n,
                        uint32_t lo, uint32_t count, const uint8_t *mem, float *out)
{
    const VertexAttr *a=d->attr;
    uintptr_t base=(uintptr_t)mem, dst=(uintptr_t)out, start, end, dend;
    uint64_t delta, src_len, dst_len; const uint8_t *p; uint32_t i;
    if (d->inline_data || n!=3 || slot[0]!=0 || slot[1]!=1 || slot[2]!=2 ||
        !a[0].offset || a[0].offset>UINT32_MAX-27u ||
        a[0].type!=NV2A_VTX_FLOAT || a[0].size!=3 || a[0].stride!=28 ||
        a[1].type!=NV2A_VTX_FLOAT || a[1].size!=2 || a[1].stride!=28 || a[1].offset!=a[0].offset+12 ||
        a[2].type!=NV2A_VTX_FLOAT || a[2].size!=2 || a[2].stride!=28 || a[2].offset!=a[0].offset+20) return 0;
    delta=(uint64_t)a[0].offset+(uint64_t)lo*28u;src_len=(uint64_t)count*28u;dst_len=(uint64_t)count*48u;
    if (delta>UINTPTR_MAX-base || src_len>UINTPTR_MAX || dst_len>UINTPTR_MAX) return 0;
    start=base+(uintptr_t)delta;
    if (src_len>UINTPTR_MAX-start || dst_len>UINTPTR_MAX-dst) return 0;
    end=start+(uintptr_t)src_len;dend=dst+(uintptr_t)dst_len;
    if (count && start<dend && dst<end) return 0;
    p=(const uint8_t *)start;
    for(i=0;i<count;++i,p+=28,out+=12) {
        float x,y,z,u,v,s,t;
        memcpy(&x,p,4);memcpy(&y,p+4,4);memcpy(&z,p+8,4);
        memcpy(&u,p+12,4);memcpy(&v,p+16,4);memcpy(&s,p+20,4);memcpy(&t,p+24,4);
        out[0]=x;out[1]=y;out[2]=z;out[3]=1.0f;
        out[4]=u;out[5]=v;out[6]=0.0f;out[7]=1.0f;
        out[8]=s;out[9]=t;out[10]=0.0f;out[11]=1.0f;
    }
    return 1;
}

/* Whole-executor equivalence and timing. Run this same executable separately
 * with AMPED_PB_CONST_FAST=0 and =1; compare the mixed-stream state hashes.
 * No window, audio device, game data or player save is opened. */
#include "kernel.h"
#include "xbox_memory_layout.h"
#include "nv2a_pb_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
extern void nv2a_pb_exec_method(uint32_t,uint32_t,uint32_t);
extern ptrdiff_t g_xbox_mem_offset;
typedef void (*guest_fn)(void);
guest_fn recomp_lookup(uint32_t va) { (void)va;return NULL; }
guest_fn recomp_lookup_manual(uint32_t va) { (void)va;return NULL; }
static uint32_t rng=0x123abc;
static uint32_t random_word(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static uint64_t hash_bytes(uint64_t h,const void *data,size_t n) {
    const unsigned char *p=data;
    while(n--) {h^=*p++;h*=0x100000001b3ull;}
    return h;
}
int main(void) {
    uint64_t state=0xcbf29ce484222325ull;
    void *mem=VirtualAlloc(NULL,64u*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!mem)return 10;
    g_xbox_mem_offset=(ptrdiff_t)mem;
    for(unsigned i=0;i<20000;i++) {
        uint32_t pick=random_word(),m,v=random_word(),sub=0;
        switch(pick%12) {
        case 0:m=0x1ea4;v%=NV2A_VSH_CONSTANTS+8;break;
        case 1:m=0x1ea0;v%=NV2A_VSH_SLOTS;break;
        case 2:m=0xb00+(pick%32)*4;break;
        case 3:m=0xa20+(pick%4)*4;break;
        case 4:m=0xaf0+(pick%4)*4;break;
        case 5:m=0x300;break;
        default:m=0xb80+(pick%32)*4;break;
        }
        if(i%29==0)sub=1;
        nv2a_pb_exec_method(sub,m,v);
        state=hash_bytes(state,nv2a_pb_vsh_state(),sizeof(struct nv2a_pb_vsh));
    }
    /* The upload stream does not mutate the large index/inline payloads.
     * Check their final state once instead of hashing gigabytes per trial. */
    state=hash_bytes(state,nv2a_pb_gpu_state(),sizeof(struct nv2a_pb_gpu));
    (printf)("STATE %016llx words=20000 constants=%u\n",(unsigned long long)state,nv2a_pb_vsh_state()->const_dwords);
    LARGE_INTEGER t0,t1,f;
    const unsigned count=10000000;
    QueryPerformanceFrequency(&f);QueryPerformanceCounter(&t0);
    for(unsigned i=0;i<count;i++) {
        if(!(i&511))nv2a_pb_exec_method(0,0x1ea4,0);
        nv2a_pb_exec_method(0,0xb80+(i%32)*4,i);
    }
    QueryPerformanceCounter(&t1);
    (printf)("TIMING ns_per_constant=%.4f count=%u\n",(double)(t1.QuadPart-t0.QuadPart)*1e9/f.QuadPart/count,count);
    VirtualFree(mem,0,MEM_RELEASE);
    return 0;
}

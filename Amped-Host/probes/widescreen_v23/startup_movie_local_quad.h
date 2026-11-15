#ifndef AMP_STARTUP_MOVIE_LOCAL_QUAD_H
#define AMP_STARTUP_MOVIE_LOCAL_QUAD_H
#include <stdint.h>
#include <string.h>
/* Contract for E7510's four contiguous 28-byte vertices. The caller supplies
 * scoped evidence and candidate assumptions for clear and shader mapping;
 * the isolated runtime experiment does not prove GPU execution or coverage. */
struct amp_local_quad_facts {uint32_t caller,outer,source_w,source_h,place_w,place_h;
    int scope_valid,source_valid,ordered_black_clear_proven,shader_mapping_proven;};
static inline int amp_local_quad_transform(uint32_t *out,const uint32_t *in,const struct amp_local_quad_facts *f,int allow_attract){
    static const uint32_t offsets[8]={0,0,0,0x43f00000u,0x44200000u,0,0x44200000u,0x43f00000u};
    unsigned i;if(!out||!in||!f||!f->scope_valid||!f->source_valid||!f->ordered_black_clear_proven||!f->shader_mapping_proven||
       f->caller!=0xE67B9u||(f->outer!=0x13A53u&&f->outer!=0x13AFCu&&f->outer!=0x13BEEu&&!(allow_attract&&f->outer==0x279FBu))||
       f->source_w!=640||f->source_h!=480||f->place_w!=640||f->place_h!=480)return 0;
    for(i=0;i<4;i++)if(in[i*7]!=offsets[i*2]||in[i*7+1]!=offsets[i*2+1]||in[i*7+3]!=0)return 0;
    memmove(out,in,28*sizeof(uint32_t));out[0]=out[7]=0x42a00000u;out[14]=out[21]=0x440c0000u;return 1;
}
static inline int amp_startup_local_quad(uint32_t *out,const uint32_t *in,const struct amp_local_quad_facts *f){return amp_local_quad_transform(out,in,f,0);}
#endif

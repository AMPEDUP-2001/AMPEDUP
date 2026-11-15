#ifndef NV2A_VK_CAPTURE_SIGNATURE_H
#define NV2A_VK_CAPTURE_SIGNATURE_H
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
/* Strict decimal width/height/format/index-count/blend/alpha selector. */
struct nvk_capture_signature { uint32_t w,h,format,indices,blend,alpha; };
static int nvk_capture_signature_parse(const char *s,struct nvk_capture_signature *out) {
    uint32_t v[6]; unsigned i; char *end;
    if(!s||!out)return 0;
    for(i=0;i<6;i++) { unsigned long n;
        if(i<3 && *s=='*') { v[i]=UINT32_MAX; ++s;
            if(*s!='/')return 0; ++s; continue; }
        if(*s<'0'||*s>'9')return 0;errno=0;n=strtoul(s,&end,10);
        if(errno || n>UINT32_MAX || (i<5 ? *end!='/' : *end!='\0'))return 0;
        v[i]=(uint32_t)n;s=end+(i<5);
    }
    if(!v[0]||!v[1]||!v[3]||v[4]>1||v[5]>1)return 0;
    *out=(struct nvk_capture_signature){v[0],v[1],v[2],v[3],v[4],v[5]};return 1;
}
static int nvk_capture_signature_match(const struct nvk_capture_signature *s,int valid,
 uint32_t w,uint32_t h,uint32_t format,uint32_t indices,uint32_t blend,uint32_t alpha) {
    return s && valid && (s->w==UINT32_MAX || s->w==w) && (s->h==UINT32_MAX || s->h==h) &&
           (s->format==UINT32_MAX || s->format==format) && s->indices==indices &&
           s->blend==!!blend && s->alpha==!!alpha;
}
#endif

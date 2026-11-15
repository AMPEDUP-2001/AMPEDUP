/* Isolated, opt-in startup movie experiment; not a general movie classifier. */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <fenv.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include "../widescreen_v23/startup_movie_local_quad.h"
extern ptrdiff_t g_xbox_mem_offset;
static _Thread_local struct { unsigned depth,valid,clear,draws,adjusted; struct amp_local_quad_facts facts; } scope;
static int flag(const char *name){const char *p=getenv(name);return p&&!strcmp(p,"1");}
#if defined(AMPED_LOADING_STRIP_V23)
extern int amp_loading_strip_direct_active(void);
#endif
#if defined(AMPED_NATIVE_WIDE_SETTING_V23)
extern int amped_native_wide_setting_active_v23;
static int setting_active(void){return amped_native_wide_setting_active_v23;}
#else
static int setting_active(void){return 0;}
#endif
static int enabled(void){return (flag("AMPED_NATIVE_WIDE_TEST")||setting_active())&&(flag("AMPED_STARTUP_QUAD_TEST")||flag("AMPED_ATTRACT_QUAD_TEST")||setting_active());}
static int read_guest(uint32_t va,void *out,size_t len){uintptr_t b=(uintptr_t)g_xbox_mem_offset;uint64_t end=(uint64_t)va+len;SIZE_T got=0;
 if(!b || !len || !((va>=0x10000u&&end<=0x04000000ull)||(va>=0x80000000u&&end<=0x84000000ull))||end>UINTPTR_MAX-b)return 0;
 return ReadProcessMemory(GetCurrentProcess(),(void*)(b+va),out,len,&got)&&got==len;}
void amp_sq_scope(unsigned obj,unsigned stack){DWORD e=GetLastError();int old=errno;uint32_t call=0,outer=0,fields[11]={0},dims[2]={0};fenv_t fp;int have=0;static atomic_flag trace_announced=ATOMIC_FLAG_INIT;
 if(enabled()){
  if(flag("AMPED_LOADING_STRIP_TRACE")&&!atomic_flag_test_and_set_explicit(&trace_announced,memory_order_relaxed)){
   have=fegetenv(&fp)==0;(fprintf)(stderr,"[AMPED_LOADING_STRIP_REJECT] trace_enabled=1 scope_entered=1 no_path_claim=1\n");}
  if(scope.depth){if(scope.depth!=UINT32_MAX)scope.depth++;scope.valid=0;}
  else {memset(&scope,0,sizeof scope);scope.depth=1;
   if(stack<=UINT32_MAX-36u&&read_guest(stack+8,&call,4)&&read_guest(stack+32,&outer,4)&&read_guest(obj,fields,sizeof fields)&&read_guest(fields[2],dims,sizeof dims)){
    /* Input-to-output shader mapping is a tested candidate, not proved here. */
    scope.facts=(struct amp_local_quad_facts){call,outer,dims[0],dims[1],fields[9],fields[10],1,1,0,1};scope.valid=1;}}
 }if(have)fesetenv(&fp);errno=old;SetLastError(e);}
void amp_sq_end(void){DWORD e=GetLastError();int old=errno;fenv_t fp;int have=0;static _Thread_local unsigned reports;
 if(scope.depth==UINT32_MAX)scope.valid=0;
 else if(scope.depth&&!--scope.depth){if(scope.adjusted && (scope.draws!=1 || (reports++%60u)==0)){have=fegetenv(&fp)==0;(fprintf)(stderr,"[AMPED_STARTUP_QUAD_TEST] outer=%08X draws=%u adjusted=%u clear_returned=%u\n",scope.facts.outer,scope.draws,scope.adjusted,scope.clear==2);}memset(&scope,0,sizeof scope);}
 if(have)fesetenv(&fp);errno=old;SetLastError(e);}
void amp_sq_clear_begin(void){if(scope.depth){if(scope.clear||scope.draws)scope.valid=0;scope.clear=1;}}
void amp_sq_clear_done(void){if(scope.depth){if(scope.clear!=1)scope.valid=0;else scope.clear=2;}}
void amp_sq_apply(unsigned va){DWORD e=GetLastError();int old=errno;fenv_t fp;int have=0;uint32_t in[28],out[28];static atomic_flag logged=ATOMIC_FLAG_INIT;
#if defined(AMPED_LOADING_STRIP_V23)
 uint32_t direct_before[28],direct_after[28];int direct=flag("AMPED_LOADING_STRIP_TRACE")&&amp_loading_strip_direct_active();
 int direct_before_ok=direct&&read_guest(va,direct_before,sizeof direct_before);static atomic_uint direct_lines;
#endif
 if(scope.depth&&enabled()){
  struct amp_local_quad_facts selected=scope.facts;
  int strip=scope.facts.caller==0x26BE1u&&flag("AMPED_LOADING_STRIP_TRACE");
  uint32_t before[28],after[28];int before_ok=strip&&read_guest(va,before,sizeof before);
  static atomic_uint strip_lines;
  if(scope.draws++ || scope.clear!=2)scope.valid=0;
  selected.ordered_black_clear_proven=scope.valid&&scope.clear==2;
  if(selected.outer==0x279FBu){if(!flag("AMPED_ATTRACT_QUAD_TEST")&&!setting_active())scope.valid=0;}
  else if(!flag("AMPED_STARTUP_QUAD_TEST")&&!setting_active())scope.valid=0;
  if(scope.valid && read_guest(va,in,sizeof in)&&amp_local_quad_transform(out,in,&selected,flag("AMPED_ATTRACT_QUAD_TEST")||setting_active())){
   unsigned i;uint8_t *p=(uint8_t*)((uintptr_t)g_xbox_mem_offset+va);
   for(i=0;i<4;i++)memcpy(p+i*28u,&out[i*7u],sizeof(uint32_t));scope.adjusted=1;
   if(!atomic_flag_test_and_set_explicit(&logged,memory_order_relaxed)){have=fegetenv(&fp)==0;(fprintf)(stderr,"[AMPED_STARTUP_QUAD_TEST] local quad adjusted; clear-call observed, execution/target unproven\n");}
  }
  if(strip && atomic_fetch_add_explicit(&strip_lines,1,memory_order_relaxed)<8){int after_ok=read_guest(va,after,sizeof after);if(!have)have=fegetenv(&fp)==0;
   (fprintf)(stderr,"[AMPED_LOADING_STRIP_REJECT] caller=00026BE1 clear_returned=%u before_valid=%d after_valid=%d unchanged=%d x=%08X,%08X,%08X,%08X observational_only=1\n",scope.clear==2,before_ok,after_ok,before_ok&&after_ok&&!memcmp(before,after,sizeof before),before_ok?before[0]:0,before_ok?before[7]:0,before_ok?before[14]:0,before_ok?before[21]:0);}
 }
#if defined(AMPED_LOADING_STRIP_V23)
 if(direct && atomic_fetch_add_explicit(&direct_lines,1,memory_order_relaxed)<8){int after_ok=read_guest(va,direct_after,sizeof direct_after);if(!have)have=fegetenv(&fp)==0;
  (fprintf)(stderr,"[AMPED_LOADING_STRIP_DIRECT_QUAD] before_valid=%d after_valid=%d unchanged=%d x=%08X,%08X,%08X,%08X observational_only=1\n",direct_before_ok,after_ok,direct_before_ok&&after_ok&&!memcmp(direct_before,direct_after,sizeof direct_before),direct_before_ok?direct_before[0]:0,direct_before_ok?direct_before[7]:0,direct_before_ok?direct_before[14]:0,direct_before_ok?direct_before[21]:0);}
#endif
 if(have)fesetenv(&fp);errno=old;SetLastError(e);}

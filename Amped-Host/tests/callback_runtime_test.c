/* Exercise the actual linked kernel dispatcher, TLS registers, gate and file
 * callbacks. All disk writes go to a disposable Windows temporary file. */
#include "kernel.h"
#include "xbox_memory_layout.h"
#include "kernel_prof.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef void (*guest_fn)(void);
extern guest_fn recomp_lookup_kernel(uint32_t);
extern RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esi,g_edi,g_esp;
extern ptrdiff_t g_xbox_mem_offset;
static uint8_t *mem;
static volatile LONG failures, callbacks;
static RECOMP_TLS int pop_bytes, nesting;
static uint32_t thunk[3];
static HANDLE file;

static uint32_t *word(uint32_t va) { return (uint32_t *)(mem+va); }
static void guest_callback(void);
guest_fn recomp_lookup(uint32_t va) { return va==0x7000u ? guest_callback : NULL; }
guest_fn recomp_lookup_manual(uint32_t va) { (void)va;return NULL; }
static void check(int ok,const char *name) {
    (printf)("%s %s\n",ok?"PASS":"FAIL",name);
    if(!ok)InterlockedIncrement(&failures);
}
static void reset_regs(uint32_t stack) {
    g_esp=stack;
    g_eax=1;g_ecx=2;g_edx=3;g_ebx=4;g_esi=5;g_edi=6;
}
static void call(unsigned slot,const uint32_t *args,unsigned n) {
    uint32_t *sp=word(g_esp);
    sp[0]=0xbeef0001;
    for(unsigned i=0;i<n;i++)sp[i+1]=args[i];
    guest_fn fn=recomp_lookup_kernel(thunk[slot]);
    if(!fn)abort();
    fn();
}
static int regs_ok(uint32_t stack,uint32_t eax) {
    return g_eax==eax && g_ecx==2 && g_edx==3 && g_ebx==4 && g_esi==5 && g_edi==6 && g_esp==stack;
}
static void read_file_apc(int nested) {
    uint32_t args[8]={(uint32_t)(uintptr_t)file,0,0x7000,0x1234,0x30000,0x31000,4,0x32000};
    reset_regs(0x20000);pop_bytes=16;nesting=nested;
    *word(0x32000)=*word(0x32004)=0;
    call(2,args,8);
    check(regs_ok(0x20024,0),nested?"real NtReadFile nested APC restores caller":"real NtReadFile APC restores caller");
    check(!memcmp(mem+0x31000,"AMP!",4),"real NtReadFile returns expected file bytes");
}
static void guest_callback(void) {
    InterlockedIncrement(&callbacks);
    if(nesting) {
        nesting=0;
        uint32_t args[3]={0,0x7000,0};
        int saved_pop=pop_bytes;
        pop_bytes=8;
        g_esp-=16;
        call(1,args,3);
        pop_bytes=saved_pop;
    }
    g_eax=0x99;g_ecx=0xaa;g_edx=0xbb;g_ebx=0xcc;g_esi=0xdd;g_edi=0xee;
    g_esp+=pop_bytes;
}
static DWORD WINAPI sync_worker(void *which) {
    uint32_t stack=0x40000+(uint32_t)(uintptr_t)which*0x1000;
    uint32_t args[3]={0,0x7000,0};
    pop_bytes=8;nesting=0;
    for(int i=0;i<1000;i++) {
        reset_regs(stack);call(1,args,3);
        if(!regs_ok(stack+16,0x99)) {InterlockedIncrement(&failures);return 1;}
    }
    return 0;
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    mem=VirtualAlloc(NULL,64u*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!mem)return 10;
    g_xbox_mem_offset=(ptrdiff_t)mem;
    const uint32_t ords[3]={232,153,219};
    for(unsigned i=0;i<3;i++)*word(0x10000+i*4)=0x80000000u|ords[i];
    xbox_kernel_set_thunk_address(0x10000,3);xbox_kernel_bridge_init();
    int profiling=xbox_ProfOn();
    (printf)("PROFILE enabled=%d\n",profiling);
    const char *expected=getenv("AMPED_TEST_PROFILE_EXPECTED");
    if(expected)check(profiling==atoi(expected),"requested profiling branch is active");
    for(unsigned i=0;i<3;i++)thunk[i]=*word(0x10000+i*4);
    uint32_t completion[3]={0x7000,0x30000,0};
    reset_regs(0x20000);pop_bytes=16;call(0,completion,3);
    check(regs_ok(0x20010,0),"completion through real kernel thunk restores registers and stack");
    uint32_t sync[3]={0,0x7000,0};
    reset_regs(0x20000);pop_bytes=8;call(1,sync,3);
    check(regs_ok(0x20010,0x99),"synchronize through real gate retains callback result");
    char dir[MAX_PATH],path[MAX_PATH];
    if(!GetTempPathA(MAX_PATH,dir)||!GetTempFileNameA(dir,"amp",0,path))return 11;
    file=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if(file==INVALID_HANDLE_VALUE||(uintptr_t)file>UINT32_MAX)return 12;
    DWORD written=0;
    check(WriteFile(file,"AMP!",4,&written,NULL)&&written==4,"temporary file prepared");
    read_file_apc(0);read_file_apc(1);
    HANDLE threads[3];
    for(uintptr_t i=0;i<3;i++)threads[i]=CreateThread(NULL,0,sync_worker,(void *)i,0,NULL);
    DWORD wait=WaitForMultipleObjects(3,threads,TRUE,15000);
    check(wait==WAIT_OBJECT_0,"three callback threads completed");
    if(wait!=WAIT_OBJECT_0)return 13;
    for(int i=0;i<3;i++) {DWORD code=1;GetExitCodeThread(threads[i],&code);check(code==0,"thread-local registers preserved for 1000 gated callbacks");CloseHandle(threads[i]);}
    check(callbacks==3005,"expected callback count including nested callback");
    CloseHandle(file);VirtualFree(mem,0,MEM_RELEASE);
    (printf)("failures=%ld callbacks=%ld\n",(long)failures,(long)callbacks);
    return failures?1:0;
}

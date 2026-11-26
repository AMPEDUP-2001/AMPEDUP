/* Real linked pacing/IRQ code, with no game or renderer. Exercise site
 * isolation, preposted wake, finite timeout, wake delivery and gate guards. */
#include "kernel.h"
#include "kernel_pacing.h"
#include "xbox_memory_layout.h"
#include <stdio.h>
#include <stdlib.h>

typedef void (*guest_fn)(void);
guest_fn recomp_lookup(uint32_t va) { (void)va; return NULL; }
guest_fn recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }
static int failures;
static volatile LONG stop;
extern ptrdiff_t g_xbox_mem_offset;
static void check(int ok,const char *what) {
    (printf)("%s %s\n",ok?"PASS":"FAIL",what);
    failures+=!ok;
}
static DWORD WINAPI wake_worker(void *unused) {
    (void)unused;
    while(!InterlockedCompareExchange(&stop,0,0)) { xbox_SpinWake(); Sleep(0); }
    return 0;
}
#ifdef _WIN32
static void check_vblank_registers(void *memory)
{
    const uint32_t gpu = 0xfd000000u;
    uint32_t *pcrtc = (uint32_t *)((char *)memory + 0x600100);
    uint32_t *pmc = (uint32_t *)((char *)memory + 0x100);
    ptrdiff_t previous = g_xbox_mem_offset;
    g_xbox_mem_offset = (ptrdiff_t)memory - gpu;
    _putenv_s("AMPED_POTATO_VBLANK_ACK", "");
    _putenv_s("AMPED_POTATO_FENCE_WAIT", "");
    xbox_AmpedConfigurePotatoTiming(0);
    check(!xbox_AmpedDirectAckEnabled() && !xbox_AmpedFenceWait(),
          "ordinary settings select stock interrupt and fence handling");
    xbox_AmpedConfigurePotatoTiming(1);
    check(xbox_AmpedDirectAckEnabled() && xbox_AmpedFenceWait(),
          "Potato Mode selects direct acknowledgements and the fence wait");
    *pcrtc = 3; *pmc = 0x01000080;
    check(!xbox_AmpedVblankWrite(gpu + 4, 1) && !xbox_AmpedVblankWrite(gpu, 2)
          && *pcrtc == 3 && *pmc == 0x01000080, "other register writes pass through");
    check(xbox_AmpedVblankWrite(gpu, 1) && *pcrtc == 2 && *pmc == 0x80,
          "acknowledgement clears only the two vertical blank bits");
    check(xbox_AmpedVblankWrite(gpu, 1) && *pcrtc == 2 && *pmc == 0x80,
          "repeated acknowledgement leaves other interrupt sources pending");
    xbox_AmpedConfigurePotatoTiming(0);
    g_xbox_mem_offset = previous;
}
#endif
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    void *mem=VirtualAlloc(NULL,64u*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!mem)return 11;
    g_xbox_mem_offset=(ptrdiff_t)mem;
    const uint32_t site=0xFE920,other=0x1234;
    XboxSpinSiteStats a,b;
    xbox_PacingSetMode(XBOX_PACING_SPIN);
    xbox_PacingSetLocalSpinSite(site);
    check(xbox_PacingMode()==XBOX_PACING_SPIN,"global pacing stays spin");
    xbox_SpinWait(other);
    check(!xbox_SpinSiteStats(other,&a),"unselected site never enters wait");
    xbox_SpinWake(); xbox_SpinWait(site);
    check(xbox_SpinSiteStats(site,&a)&&a.immediate==1,"preposted wake is retained in spin mode");
    xbox_SpinWait(site);
    check(xbox_SpinSiteStats(site,&a)&&a.timeouts==1,"no writer yields a bounded timeout");
    int saved=xbox_IrqlEnterInterrupt(DISPATCH_LEVEL);
    xbox_SpinWait(site);
    check(xbox_SpinSiteStats(site,&b)&&b.dispatch==a.dispatch+1&&b.timeouts==a.timeouts,"DISPATCH guard never sleeps");
    xbox_IrqlLeaveInterrupt(saved);
    xbox_DispatchGateEnter(); xbox_SpinWait(site);
    check(xbox_SpinSiteStats(site,&a)&&a.dispatch==b.dispatch+1,"gate holder never sleeps");
    xbox_DispatchGateLeave();
    HANDLE worker=CreateThread(NULL,0,wake_worker,NULL,0,NULL);
    if(!worker)return 10;
    for(unsigned i=0;i<1000;i++) xbox_SpinWait(site);
    InterlockedExchange(&stop,1);
    check(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0,"wake producer completed");
    CloseHandle(worker);
    check(xbox_SpinSiteStats(site,&a)&&a.wakes>0,"blocked local waiter receives broadcasts with global spin mode");
    xbox_PacingSetLocalSpinSite(0);
    check(xbox_PacingLocalSpinSite()==0,"local site selection can be disabled");
    xbox_SpinWait(site); xbox_SpinWait(0);
    check(xbox_SpinSiteStats(site,&b)&&a.waits==b.waits&&!xbox_SpinSiteStats(0,&b),"zero disables local sleeping");
    const uint32_t fence = 0x000feae4u;
    xbox_AmpedSetFenceWait(1);
    xbox_SpinWake();
    xbox_SpinWait(fence);
    check(xbox_SpinSiteStats(fence,&a) && a.immediate == 1, "fence wait receives an already posted wake");
    xbox_SpinWait(other);
    check(!xbox_SpinSiteStats(other,&b), "fence wait is scoped to the matched guest instruction");
    xbox_DispatchGateEnter(); xbox_SpinWait(fence); xbox_DispatchGateLeave();
    check(xbox_SpinSiteStats(fence,&b) && b.dispatch == 1 && !b.timeouts, "a fence gate holder never sleeps");
    xbox_AmpedSetFenceWait(0);
    xbox_PacingSetMode(XBOX_PACING_SLEEP); xbox_SpinWake(); xbox_SpinWait(other);
    check(xbox_SpinSiteStats(other,&b)&&b.waits==1,"global sleep remains functional");
#ifdef _WIN32
    check_vblank_registers(mem);
#endif
    return failures?1:0;
}

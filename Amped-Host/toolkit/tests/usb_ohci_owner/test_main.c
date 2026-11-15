/* Exercises the real OHCI interrupt-transfer routine and the kernel's real
 * physical-owner registry. The pad and mapped guest RAM are test fixtures. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/usb/ohci.c"

static uint8_t *ram;
static uint8_t pages[OHCI_CONTIG_SIZE / 4096];
uint32_t g_xbox_image_lo, g_xbox_image_hi;
void xbox_log(int level, const char *subsystem, const char *fmt, ...)
{ (void)level; (void)subsystem; (void)fmt; }

ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)ram; }
size_t xbox_GetMappedSize(void) { return OHCI_CONTIG_SIZE; }
const volatile uint8_t *xbox_ContiguousPageMap(void) { return pages; }
uint32_t xbox_ContiguousAllocatedBytes(void) { return 0; }
uint8_t usb_gamepad_address(int pad) { (void)pad; return 0; }
int usb_gamepad_report(int pad, uint8_t *out, int max)
{
    (void)pad;
    if (max < 20) return 0;
    memset(out, 0x5a, 20);
    return 20;
}
int usb_gamepad_control(int pad, const UsbSetup *s, uint8_t *out, int max)
{ (void)pad; (void)s; (void)out; (void)max; return -1; }
void usb_gamepad_output(int pad, const uint8_t *out, int n)
{ (void)pad; (void)out; (void)n; }

static int run_case(const char *name, uint32_t p, uint32_t owner, uint32_t owner_end, int enabled,
                    uint32_t expected)
{
    const uint32_t td = 0x10000;
    uint8_t *low = ram + p, *high = ram + OHCI_CONTIG_BASE + p;
    OhciController hc = {0};
    uint32_t result;
    memset(low, 0xa1, 20);
    memset(high, 0xb2, 20);
    pages[p >> 12] = 1;
    pages[(p + 19) >> 12] = 1;
    if (owner) xbox_MmGetPhysicalAddress((void *)(uintptr_t)owner);
    if (owner_end) xbox_MmGetPhysicalAddress((void *)(uintptr_t)owner_end);
    _putenv_s("AMPED_OHCI_OWNER_REPORT", enabled ? "1" : "");
    *(uint32_t *)(ram + td) = TD_DP_IN << 19;
    *(uint32_t *)(ram + td + 4) = p;
    *(uint32_t *)(ram + td + 12) = p + 19;
    result = ohci_do_td(&hc, 1u << 7, td);
    int ok = result == TD_CC_NOERROR && *(uint32_t *)(ram + td + 4) == 0;
    for (unsigned i = 0; i < 20; ++i) {
        ok &= low[i] == (expected == p ? 0x5a : 0xa1);
        ok &= high[i] == (expected == OHCI_CONTIG_BASE + p ? 0x5a : 0xb2);
    }
    printf("%s: %s\n", name, ok ? "PASS" : "FAIL");
    return !ok;
}

int main(void)
{
    const uint32_t p = 0x0110438a;
    int fails = 0;
    ram = VirtualAlloc(NULL, (size_t)OHCI_CONTIG_BASE + OHCI_CONTIG_SIZE,
                       MEM_RESERVE, PAGE_NOACCESS);
    if (!ram || !VirtualAlloc(ram + 0x10000, 4096, MEM_COMMIT, PAGE_READWRITE) ||
        !VirtualAlloc(ram + (p & ~4095u), 8192, MEM_COMMIT, PAGE_READWRITE) ||
        !VirtualAlloc(ram + OHCI_CONTIG_BASE + (p & ~4095u), 8192,
                      MEM_COMMIT, PAGE_READWRITE)) {
        fprintf(stderr, "failed to reserve test guest memory\n");
        return 2;
    }
    s_plugged_pads = 1;
    fails += run_case("unregistered fallback", p, 0, 0, 1, OHCI_CONTIG_BASE + p);
    fails += run_case("low registered owner", p, p, 0, 1, p);
    fails += run_case("high registered owner", p, OHCI_CONTIG_BASE + p, 0, 1,
                      OHCI_CONTIG_BASE + p);
    fails += run_case("opt-in disabled", p, p, 0, 0, OHCI_CONTIG_BASE + p);
    const uint32_t cross = 0x01104ff8;
    fails += run_case("cross-page low owners", cross, cross, cross + 19, 1, cross);
    fails += run_case("cross-page mismatched owners fallback", cross, cross,
                      OHCI_CONTIG_BASE + cross + 19, 1, OHCI_CONTIG_BASE + cross);
    VirtualFree(ram, 0, MEM_RELEASE);
    return fails ? 1 : 0;
}

/* Host-only harness for the actual guest stack allocator functions. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static volatile LONG next_base = 0x01000000;
static volatile LONG fail_heap, barrier_enabled, barrier_arrived;
static HANDLE both_in_heap;

uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment)
{
    (void)alignment;
    if (InterlockedCompareExchange(&fail_heap, 0, 0)) return 0;
    if (InterlockedCompareExchange(&barrier_enabled, 0, 0)) {
        if (InterlockedIncrement(&barrier_arrived) == 2) SetEvent(both_in_heap);
        /* A correct allocator reserves the last slot before entering the heap,
         * so the second call cannot arrive. Continue after a bounded wait. */
        WaitForSingleObject(both_in_heap, 1000);
    }
    return (uint32_t)InterlockedExchangeAdd(&next_base, (LONG)(size + 4096));
}
void xbox_HeapFree(uint32_t base) { (void)base; }

/* The runner substitutes the allocator constants, state and functions from
 * xbox_memory_layout.c at this marker; no production code is changed. */
/* ACTUAL_ALLOCATOR */

static DWORD WINAPI allocate_one(void *out)
{
    *(uint32_t *)out = xbox_AllocThreadStack();
    return 0;
}
static int check(int condition, const char *label)
{
    printf("%s: %s\n", label, condition ? "PASS" : "FAIL");
    return !condition;
}
int main(void)
{
    uint32_t stacks[32] = {0}, result[2] = {0};
    HANDLE threads[2];
    int failures = 0;
    _putenv_s("AMPED_MORE_THREAD_STACKS", "");
    for (int i = 0; i < 8; ++i) stacks[i] = xbox_AllocThreadStack();
    int distinct = 1;
    for (int i = 0; i < 8; ++i) {
        distinct &= stacks[i] != 0 && (stacks[i] & 15) == 0;
        for (int j = 0; j < i; ++j) distinct &= stacks[i] != stacks[j];
    }
    failures += check(distinct && !xbox_AllocThreadStack(), "default limit and distinct stacks");
    xbox_FreeThreadStack(stacks[0]);
    stacks[0] = xbox_AllocThreadStack();
    failures += check(stacks[0] != 0 && !xbox_AllocThreadStack(), "free restores one slot");
    for (int i = 0; i < 8; ++i) xbox_FreeThreadStack(stacks[i]);
    fail_heap = 1;
    failures += check(xbox_AllocThreadStack() == 0, "heap failure is reported");
    fail_heap = 0;
    _putenv_s("AMPED_MORE_THREAD_STACKS", "1");
    for (int i = 0; i < 32; ++i) stacks[i] = xbox_AllocThreadStack();
    failures += check(stacks[31] != 0 && !xbox_AllocThreadStack(), "opt-in limit and failed allocation leave quota intact");
    for (int i = 0; i < 32; ++i) xbox_FreeThreadStack(stacks[i]);
    _putenv_s("AMPED_MORE_THREAD_STACKS", "");
    for (int i = 0; i < 7; ++i) stacks[i] = xbox_AllocThreadStack();
    both_in_heap = CreateEvent(NULL, TRUE, FALSE, NULL);
    barrier_enabled = 1;
    threads[0] = CreateThread(NULL, 0, allocate_one, &result[0], 0, NULL);
    threads[1] = CreateThread(NULL, 0, allocate_one, &result[1], 0, NULL);
    if (!both_in_heap || !threads[0] || !threads[1]) return 2;
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    barrier_enabled = 0;
    failures += check((result[0] != 0) + (result[1] != 0) == 1,
                      "concurrent allocation cannot exceed limit");
    for (int i = 0; i < 2; ++i) { xbox_FreeThreadStack(result[i]); CloseHandle(threads[i]); }
    for (int i = 0; i < 7; ++i) xbox_FreeThreadStack(stacks[i]);
    CloseHandle(both_in_heap);
    return failures ? 1 : 0;
}

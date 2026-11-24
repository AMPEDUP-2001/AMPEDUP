/* Synthetic guest-memory slot for exercising LLDB without a game disc. */
#include <stdint.h>
#include <stdio.h>

static volatile uint32_t callback;
uintptr_t g_xbox_mem_offset;

__attribute__((noinline)) static void xbox_kernel_init(void)
{
    puts("mapping ready");
}

__attribute__((noinline)) static void set_callback(uint32_t value)
{
    callback = value;
}

int main(void)
{
    g_xbox_mem_offset = (uintptr_t)&callback - 0x00242c4c;
    xbox_kernel_init();
    set_callback(42);
    set_callback(0);
    puts("fixture finished");
    return callback != 0;
}

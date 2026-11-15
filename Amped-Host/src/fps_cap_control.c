#include "fps_cap_control.h"
#include <stdatomic.h>
static atomic_uint s_hz = ATOMIC_VAR_INIT(0);
void amped_fps_cap_set(unsigned hz) {
 if(hz!=30 && hz!=60 && hz!=120) hz=0;
 atomic_store_explicit(&s_hz,hz,memory_order_release);
}
unsigned amped_fps_cap_get(void) { return atomic_load_explicit(&s_hz,memory_order_acquire); }

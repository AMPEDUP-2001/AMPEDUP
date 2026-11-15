#ifndef AMPED_FLIP_CAP_H
#define AMPED_FLIP_CAP_H
#include <stdint.h>
#include <string.h>
/* Host flip cap, not a simulation clock. Caller owns state on executor thread. */
typedef struct { uint64_t due; uint32_t last_flip; unsigned last_hz; int have_flip; } amped_flip_cap;
static unsigned amped_flip_cap_parse(const char *s) {
 if (!s || !strcmp(s,"off") || !strcmp(s,"0")) return 0;
 if (!strcmp(s,"30")) return 30; if (!strcmp(s,"60")) return 60; if (!strcmp(s,"120")) return 120; return 0;
}
static uint64_t amped_flip_cap_due(amped_flip_cap *s,uint32_t flip,uint64_t now,unsigned hz) {
 uint64_t period;
 if (s->last_hz != hz) { s->due=0; s->have_flip=0; s->last_hz=hz; }
 if (!hz) { s->due=0; s->have_flip=0; return now; }
 if (s->have_flip && s->last_flip==flip) return now;
 period=1000000000ull/hz;s->last_flip=flip;s->have_flip=1;
 if (!s->due || (now>s->due && now-s->due>period*4u) || s->due>UINT64_MAX-period) { s->due=now; }
 else if (now<s->due) now=s->due;
 s->due=now+period;return now;
}
#if defined(AMPED_CAP_PHASE_TEST)
/* Keep the deadline grid through a fractional-frame miss. When given the
 * previous release time, recover at most 1 ms per frame. */
static uint64_t amped_flip_cap_due_phase(amped_flip_cap *s,uint32_t flip,uint64_t now,
                                         unsigned hz,uint64_t previous_release) {
 const uint64_t recovery_ns = 1000000ull;
 uint64_t period, target;
 if (s->last_hz != hz) previous_release=0;
 if (s->last_hz != hz) { s->due=0; s->have_flip=0; s->last_hz=hz; }
 if (!hz) { s->due=0; s->have_flip=0; return now; }
 if (s->have_flip && s->last_flip==flip) return now;
 period=1000000000ull/hz;s->last_flip=flip;s->have_flip=1;
 if (!s->due || (now>s->due && now-s->due>=period) || s->due>UINT64_MAX-period) s->due=now;
 if (s->due>UINT64_MAX-period) { s->due=0; return now; }
 target = now < s->due ? s->due : now;
 s->due += period;
 if (previous_release && period > recovery_ns &&
     previous_release <= UINT64_MAX - (period - recovery_ns)) {
     uint64_t earliest = previous_release + (period - recovery_ns);
     if (target < earliest) target = earliest;
 }
 return target;
}
#endif
#endif

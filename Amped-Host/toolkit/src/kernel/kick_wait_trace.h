#ifndef AMPED_KICK_WAIT_TRACE_H
#define AMPED_KICK_WAIT_TRACE_H
#include <stdint.h>
/* Outcome: 0 generation already changed, 1 woke, 2 timeout/no generation. */
uint64_t xbox_KickWaitTraceBegin(void);
void xbox_KickWaitTraceEnd(uint64_t started, unsigned outcome);
void xbox_KickWaitTraceFlip(uint32_t flip);
void xbox_KickWaitTraceCleared(void);
#ifdef KICK_WAIT_TRACE_TEST
void xbox_KickWaitTraceTestSetEpoch(uint64_t value);
uint64_t xbox_KickWaitTraceTestGetEpoch(void);
#endif
#endif

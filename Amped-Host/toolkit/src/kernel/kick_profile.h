#ifndef XBOX_KICK_PROFILE_H
#define XBOX_KICK_PROFILE_H
#include <stdint.h>

/* Opt-in aggregate observation only. A zero start means disabled. */
uint64_t xbox_KickProfBegin(uint64_t *scan_sequence);
void xbox_KickProfEnd(uint64_t start_ns, uint64_t scan_sequence);
uint64_t xbox_KickProfScanBegin(void);
void xbox_KickProfScanEnd(uint64_t start_ns);
#endif

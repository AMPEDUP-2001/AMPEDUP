#ifndef AMPED_GATHER_FAST_H
#define AMPED_GATHER_FAST_H
#include "kernel/nv2a_backend_common.h"
/* Returns 1 only when this exact layout was gathered. Otherwise writes nothing. */
int amped_gather_fast(const struct nv2a_pb_draw *d, const int *slot, int n,
                      uint32_t lo, uint32_t count, const uint8_t *mem, float *out);
/* Isolated experiment for the Snow Summit 28-byte layout. */
int amped_gather_snow(const struct nv2a_pb_draw *d, const int *slot, int n,
                      uint32_t lo, uint32_t count, const uint8_t *mem, float *out);
/* Optional four-attribute Snow Summit layout; identical fallback contract. */
int amped_gather_snow4(const struct nv2a_pb_draw *d, const int *slot, int n,
                       uint32_t lo, uint32_t count, const uint8_t *mem, float *out);
/* Optional position/two-UV stride-28 shape; generic gather remains fallback. */
int amped_gather_snowuv(const struct nv2a_pb_draw *d, const int *slot, int n,
                        uint32_t lo, uint32_t count, const uint8_t *mem, float *out);
#endif

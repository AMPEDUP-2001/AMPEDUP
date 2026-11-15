#ifndef AMPED_GATHER_TRACE_H
#define AMPED_GATHER_TRACE_H
#include <stdint.h>
#include "kernel/nv2a_pb_state.h"
/* Tokens are zero for disabled/unsampled draws. Executor thread only. */
uint64_t amped_gather_trace_begin(void);
uint64_t amped_gather_trace_split(uint64_t start);
void amped_gather_trace_end(uint64_t start, uint64_t split,
                            const struct nv2a_pb_draw *draw, const int *slots,
                            unsigned nslots, uint32_t vertices, uint32_t indices);
void amped_gather_trace_flip(uint64_t present);
#endif

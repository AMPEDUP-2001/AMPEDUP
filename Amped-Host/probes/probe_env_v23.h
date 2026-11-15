#ifndef AMPED_PROBE_ENV_V23_H
#define AMPED_PROBE_ENV_V23_H
/* With AMPED_CACHE_GUEST_PROBES=1, snapshot known probe options on the first
 * probe call. Otherwise preserve ordinary getenv semantics. */
const char *amped_probe_getenv(const char *name);
#endif

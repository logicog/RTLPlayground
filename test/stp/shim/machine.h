#ifndef _SHIM_MACHINE_H_
#define _SHIM_MACHINE_H_
#include <stdint.h>
#define MACHINE_MIN_PORT 0
#define MACHINE_MAX_PORT 8
#define MACHINE_PORT_VALID(p) \
	((uint8_t)((p) - MACHINE_MIN_PORT) <= (MACHINE_MAX_PORT - MACHINE_MIN_PORT))
struct machine { uint8_t log_to_phys_port[9]; };
extern const struct machine machine;
struct machine_runtime { uint8_t isRTL8373; };
extern struct machine_runtime machine_detected;
#endif

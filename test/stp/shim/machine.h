#ifndef _SHIM_MACHINE_H_
#define _SHIM_MACHINE_H_
#include <stdint.h>
struct machine { uint8_t min_port; uint8_t max_port; uint8_t log_to_phys_port[9]; };
extern const struct machine machine;
struct machine_runtime { uint8_t isRTL8373; };
extern struct machine_runtime machine_detected;
#endif

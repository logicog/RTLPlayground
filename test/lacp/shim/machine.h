/* Host-build shim: rtl837x_lacp.c reads min_port/max_port and, for the
 * console dump, the front-panel numbering. The real map is not the identity
 * on any machine, so the harness uses a shuffled one to keep a logical index
 * from passing as a physical port by accident. */
#ifndef _SHIM_MACHINE_H_
#define _SHIM_MACHINE_H_
#include <stdint.h>
struct machine { uint8_t min_port; uint8_t max_port; uint8_t log_to_phys_port[9]; };
extern struct machine machine;
struct machine_runtime { uint8_t isRTL8373; };
extern struct machine_runtime machine_detected;
#endif

/* Host-build shim: rtl837x_lacp.c reads only min_port/max_port */
#ifndef _SHIM_MACHINE_H_
#define _SHIM_MACHINE_H_
#include <stdint.h>
struct machine { uint8_t min_port; uint8_t max_port; };
extern struct machine machine;
#endif

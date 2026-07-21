/* Sandbox shim: only what rtl837x_lacp.c pulls from the STP module. */
#ifndef _RTL837X_STP_H_
#define _RTL837X_STP_H_
#include <stdint.h>
signed char cmpMAC(uint8_t *m1, uint8_t *m2);
#endif

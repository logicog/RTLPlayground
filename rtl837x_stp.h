#ifndef _RTL837X_STP_H_
#define _RTL837X_STP_H_

#include <stdint.h>
void stp_in(void) __banked;
void stp_setup(void) __banked;
void stp_timers(void) __banked;
void stp_off(void) __banked;

/* 6-byte MAC/system-ID compare (defined in rtl837x_stp.c, shared with LACP -
 * both modules live in code BANK2, so the call is intra-bank). */
signed char cmpMAC(__xdata uint8_t *m1, __xdata uint8_t *m2) __reentrant;

#define TIME_HELLO 0x200 // 2 sec

#endif

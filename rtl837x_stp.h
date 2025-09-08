#ifndef _RTL837X_STP_H_
#define _RTL837X_STP_H_

#include <stdint.h>
void stp_in(void) __banked;
void stp_setup(void) __banked;
void stp_timers(void) __banked;
void stp_off(void) __banked;

#define TIME_HELLO 0x200 // 2 sec

#endif

#ifndef _LLDP_H_
#define _LLDP_H_

#include <stdint.h>
void lldp_in(void) __banked;
void lldp_setup(void) __banked;
void lldp_timers(void) __banked;
void lldp_off(void) __banked;

#endif

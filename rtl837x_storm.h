#ifndef _RTL837X_STORM_H_
#define _RTL837X_STORM_H_

#include <stdint.h>

#define STORM_TYPES	4
#define STORM_METER_BASE	24
#define STORM_METER(port, type)	(STORM_METER_BASE + (((port) << 2) | (type)))

void storm_set(uint8_t port, __xdata uint8_t type, __xdata uint32_t rate, __xdata uint8_t pps) __banked;
void storm_off(uint8_t port, __xdata uint8_t type) __banked;
void storm_show(void) __banked;

#endif

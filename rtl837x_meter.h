#ifndef _RTL837X_METER_H_
#define _RTL837X_METER_H_

#include <stdint.h>

#define METERS		64
#define METER_NONE	0xff

uint8_t meter_claim(uint8_t m) __banked;
uint8_t meter_alloc(void) __banked;
void meter_release(uint8_t m) __banked;

#endif

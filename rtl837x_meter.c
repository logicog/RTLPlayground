#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_meter.h"

#pragma codeseg BANK3
#pragma constseg BANK3

static __xdata uint8_t meter_used[METERS / 8];
static __xdata uint8_t meter_i;


void meter_init(void) __banked
{
	for (meter_i = 0; meter_i < METERS / 8; meter_i++)
		meter_used[meter_i] = 0;
}


uint8_t meter_alloc(void) __banked
{
	meter_i = METERS;
	while (meter_i--) {
		if (!(meter_used[meter_i >> 3] & (1 << (meter_i & 7)))) {
			meter_used[meter_i >> 3] |= 1 << (meter_i & 7);
			return meter_i;
		}
	}
	return METER_NONE;
}


void meter_release(uint8_t m) __banked
{
	meter_used[m >> 3] &= ~(1 << (m & 7));
}

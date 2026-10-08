#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_meter.h"

#pragma codeseg BANK3
#pragma constseg BANK3

static __xdata uint8_t meter_used[METERS / 8];
static __xdata uint8_t meter_i;


uint8_t meter_claim(uint8_t m) __banked
{
	if (meter_used[m >> 3] & (1 << (m & 7)))
		return 0;
	meter_used[m >> 3] |= 1 << (m & 7);
	return 1;
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

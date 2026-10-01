#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_storm.h"
#include "rtl837x_meter.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];

static __xdata uint32_t storm_w;
static __xdata uint8_t storm_shift;

static __code char * __code storm_names[STORM_TYPES] = { " bcast ", " mcast ", " ucast ", " umcast " };


static void storm_midx_read(uint8_t port, __xdata uint8_t type)
{
	reg_read_m(RTL837X_STORM_MIDX + (type << 3) + ((port / 5) << 2));
	storm_w = ((uint32_t)sfr_data[0] << 24) | ((uint32_t)sfr_data[1] << 16)
		  | ((uint16_t)sfr_data[2] << 8) | sfr_data[3];
	storm_shift = (port % 5) * 6;
}


uint8_t storm_meter(uint8_t port, __xdata uint8_t type) __banked
{
	storm_midx_read(port, type);
	return (storm_w >> storm_shift) & 0x3f;
}


uint8_t storm_set(uint8_t port, __xdata uint8_t type, __xdata uint32_t rate, __xdata uint8_t pps) __banked
{
	__xdata uint8_t idx;
	__xdata uint8_t * __xdata r = (uint8_t *)&rate;

	if (reg_bit_test(RTL837X_STORM_CTRL + (type << 2), port))
		idx = storm_meter(port, type);
	else if ((idx = meter_alloc()) == METER_NONE)
		return 0;

	sfr_data[0] = 0;
	sfr_data[1] = r[2];
	sfr_data[2] = r[1];
	sfr_data[3] = r[0];
	reg_write_m(RTL837X_METER_RATE + (idx << 2));

	if (!pps) {
		sfr_data[1] = 0;
		sfr_data[2] = 0x20;
		sfr_data[3] = 0;
	}
	reg_write_m(RTL837X_METER_BURST + (idx << 2));

	if (pps)
		reg_bit_set(RTL837X_METER_MODE + ((idx >> 5) << 2), idx & 0x1f);
	else
		reg_bit_clear(RTL837X_METER_MODE + ((idx >> 5) << 2), idx & 0x1f);

	storm_midx_read(port, type);
	storm_w &= ~((uint32_t)0x3f << storm_shift);
	storm_w |= (uint32_t)idx << storm_shift;
	sfr_data[0] = storm_w >> 24;
	sfr_data[1] = storm_w >> 16;
	sfr_data[2] = storm_w >> 8;
	sfr_data[3] = storm_w;
	reg_write_m(RTL837X_STORM_MIDX + (type << 3) + ((port / 5) << 2));

	reg_bit_set(RTL837X_STORM_CTRL + (type << 2), port);
	return 1;
}


void storm_off(uint8_t port, __xdata uint8_t type) __banked
{
	if (!reg_bit_test(RTL837X_STORM_CTRL + (type << 2), port))
		return;
	meter_release(storm_meter(port, type));
	reg_bit_clear(RTL837X_STORM_CTRL + (type << 2), port);
}


void storm_show(void) __banked
{
	uint8_t i, t, idx;

	for (i = machine.min_port; i <= machine.max_port; i++) {
		print_string("port ");
		write_char('0' + machine.log_to_phys_port[i]);
		for (t = 0; t < STORM_TYPES; t++) {
			print_string(storm_names[t]);
			if (!reg_bit_test(RTL837X_STORM_CTRL + (t << 2), i)) {
				print_string("off");
				continue;
			}
			idx = storm_meter(i, t);
			reg_read_m(RTL837X_METER_RATE + (idx << 2));
			print_byte(sfr_data[1]); print_byte(sfr_data[2]); print_byte(sfr_data[3]);
			if (reg_bit_test(RTL837X_METER_MODE + ((idx >> 5) << 2), idx & 0x1f))
				print_string(" pps");
			else
				print_string(" kbps");
		}
		write_char('\n');
	}
}

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_storm.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];

/* Meter index fields of RTL837X_STORM_MIDX, [type << 1 | ports 5-9] */
static const __code uint8_t storm_midx[8][4] = {
	{ 0x10, 0x30, 0x81, 0x00 }, { 0x24, 0x81, 0xc6, 0x14 },
	{ 0x11, 0x34, 0x91, 0x41 }, { 0x25, 0x85, 0xd6, 0x55 },
	{ 0x12, 0x38, 0xa1, 0x82 }, { 0x26, 0x89, 0xe6, 0x96 },
	{ 0x13, 0x3c, 0xb1, 0xc3 }, { 0x27, 0x8d, 0xf6, 0xd7 },
};

static __code char * __code storm_names[STORM_TYPES] = { " bcast ", " mcast ", " ucast ", " umcast " };


void storm_set(uint8_t port, __xdata uint8_t type, __xdata uint32_t rate, __xdata uint8_t pps) __banked
{
	__xdata uint8_t idx = (port << 2) | type;
	__xdata uint8_t row = (type << 1) | (port >= 5 ? 1 : 0);
	__xdata uint8_t * __xdata r = (uint8_t *)&rate;

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

	sfr_data[0] = storm_midx[row][0];
	sfr_data[1] = storm_midx[row][1];
	sfr_data[2] = storm_midx[row][2];
	sfr_data[3] = storm_midx[row][3];
	reg_write_m(RTL837X_STORM_MIDX + (row << 2));

	reg_bit_set(RTL837X_STORM_CTRL + (type << 2), port);
}


void storm_off(uint8_t port, __xdata uint8_t type) __banked
{
	reg_bit_clear(RTL837X_STORM_CTRL + (type << 2), port);
}


void storm_show(void) __banked
{
	__xdata uint8_t i, t, idx;

	for (i = machine.min_port; i <= machine.max_port; i++) {
		print_string("port ");
		write_char('0' + machine.log_to_phys_port[i]);
		for (t = 0; t < STORM_TYPES; t++) {
			print_string(storm_names[t]);
			if (!reg_bit_test(RTL837X_STORM_CTRL + (t << 2), i)) {
				print_string("off");
				continue;
			}
			idx = (i << 2) | t;
			reg_read_m(RTL837X_METER_RATE + (idx << 2));
			print_byte(sfr_data[1]); print_byte(sfr_data[2]); print_byte(sfr_data[3]);
			if (reg_bit_test(RTL837X_METER_MODE + ((idx >> 5) << 2), idx & 0x1f))
				print_string(" pps");
			else
				print_string(" x16kbps");
		}
		write_char('\n');
	}
}

/*
 * "temp" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_temp.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"

#pragma codeseg BANK2
#pragma constseg BANK2

static void print_temperature(uint16_t reg)
{
	int16_t v;

	reg_read(reg);
	v = SFR_DATA_U16;
	if (v < 0) {
		write_char('-');
		v = -v;
	}
	itoa(v >> 7);
	write_char('.');
	itoa(((v & 0x7f) * 10) >> 7);
	print_string(" C\n");
}

void cmd_parse_temp(void)
{
	print_string("\nChip temperature: ");
	print_temperature(RTL837X_TM_RESULT);
	print_string("At power-on:      ");
	print_temperature(RTL837X_TM_RESULT_POWERON);
}

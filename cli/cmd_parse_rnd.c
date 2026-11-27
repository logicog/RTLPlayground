/*
 * "rnd" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_rnd.h"
#include "rtl837x_regs.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern volatile __xdata uint8_t sfr_data[4];

void cmd_parse_rnd(void)
{
	// In order to get a new random numner, this bit has to be set each time!
	reg_bit_set(RTL837X_RLDP_RLPP, RLDP_RND_EN);
	reg_read_m(RTL837X_RAND_NUM1);
	print_byte(sfr_data[2]);
	print_byte(sfr_data[3]);
	reg_read_m(RTL837X_RAND_NUM0);
	print_byte(sfr_data[0]);
	print_byte(sfr_data[1]);
	print_byte(sfr_data[2]);
	print_byte(sfr_data[3]);
	write_char('\n');
}

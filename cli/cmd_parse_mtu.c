/*
 * "mtu" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_mtu.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_mtu(void)
{
	uint8_t p;

	if (cmd_compare(1, "show")) {
		for (p = MACHINE_MIN_PORT; p <= MACHINE_MAX_PORT; p++) {
			reg_read_m(RTL8373_REG_MAC_L2_PORT_MAX_LEN + ((uint16_t) p << 8));
			uint16_t mtu = SFR_DATA_U16 & 0x3fff;
			print_string("Port "); print_phys_port(p);
			write_char(' '); print_short(mtu); write_char('\n');
		}
		return;
	}
	if (cmd_words_len != 3)
		goto err;

	if (cmd_arg_port_separator(cmd_words_b[1]) == 0)
		goto err;

	p = atoi_results_u8;
	print_byte(p);

	if (atoi_short(cmd_words_b[2]) == 0 || atoi_results_short < 64 || atoi_results_short > 0x3fff) {
		cmd_error("MTU must be 64..16383\n");
		return;
	}
	REG_WRITE(RTL8373_REG_MAC_L2_PORT_MAX_LEN + ((uint16_t) p << 8), (atoi_results_short >> 10) & 0xf, (atoi_results_short >> 2) & 0xff,
		  ((atoi_results_short & 0x3) << 6) | ((atoi_results_short >> 8) & 0x3f), atoi_results_short & 0xff);
	write_char('\n');
	return;

err:
	cmd_error("mtu [port] [size]\n");
	return;
}

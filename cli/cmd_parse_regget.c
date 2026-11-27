/*
 * "regget" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_regget.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_regget(void)
{
	if (cmd_words_len != 2) {
		goto err;
	}

	uint8_t hex_size = atoi_hex(cmd_words_b[1]);

	if (hex_size == 0 || hex_size > 2) {
		goto err;
	}

	uint16_t reg = hexvalue[0];
	if (hex_size == 2) {
		reg <<= 8;
		reg |= hexvalue[1];
	}

	print_string("REGGET: ");
	print_short(reg);
	print_string(": VAL: ");

	reg_read_m(reg);
	print_sfr_data();
	write_char('\n');
	return;

err:
	cmd_error("regget <hexvalue>\n\tlike: regget 0BB0 or regget 0c");
	return;
}

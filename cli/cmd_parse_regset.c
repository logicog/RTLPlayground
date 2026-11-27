/*
 * "regset" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_regset.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern volatile __xdata uint8_t sfr_data[4];

void cmd_parse_regset(void)
{
	if (cmd_words_len != 3) {
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

	hex_size = atoi_hex(cmd_words_b[2]);
	if (hex_size == 0 || hex_size > 4) {
		goto err;
	}

	// zero sfr memory data
	sfr_set_zero();

	// copy data over sfr memory
	uint8_t offset = 4 - hex_size;
	while(hex_size) {
		hex_size -= 1;
		sfr_data[offset + hex_size] = hexvalue[hex_size];
	}
	print_string("REGSET: ");
	print_short(reg);

	reg_write_m(reg);

	print_string(": VAL: ");
	print_sfr_data();
	write_char('\n');
	return;

err:
	cmd_error("regset <hexvalue> <hexvalue>\n\tlike regset 0b abcd1234.");
}

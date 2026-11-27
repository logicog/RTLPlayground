/*
 * "physet" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_physet.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_physet(void)
{
	__xdata uint8_t phy_id, dev_id, hex_size;
	__xdata uint16_t reg, val;

	if (cmd_words_len != 5) {
		goto err;
	}

	if (!atoi_byte(cmd_words_b[1])) {
		goto err;
	}
	phy_id = atoi_results_u8;


	if (!atoi_byte(cmd_words_b[2])) {
		goto err;
	}
	dev_id = atoi_results_u8;

	hex_size = atoi_hex(cmd_words_b[3]);
	if (hex_size == 0 || hex_size > 2) {
		goto err;
	}
	reg = hexvalue[0];
	if (hex_size == 2) {
		reg <<= 8;
		reg |= hexvalue[1];
	}

	hex_size = atoi_hex(cmd_words_b[4]);
	if (hex_size == 0 || hex_size > 2) {
		goto err;
	}
	val = hexvalue[0];
	if (hex_size == 2) {
		val <<= 8;
		val |= hexvalue[1];
	}

	print_string("PHYSET: ");
	itoa(phy_id);
	print_string(":");
	itoa(dev_id);
	print_string(".");
	print_short(reg);
	print_string(": VAL: ");

	phy_write(phy_id, dev_id, reg, val);

	print_short(val);
	write_char('\n');
	return;

err:
	cmd_error("physet <phy-id> <dev-id> <hex:reg> <hex:val>\n");
	return;
}

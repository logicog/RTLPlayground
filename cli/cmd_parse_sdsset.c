/*
 * "sdsset" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_sdsset.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_sdsset(void)
{
	__xdata uint8_t sds_id, page, reg, hex_size;
	__xdata uint16_t val;

	if (cmd_words_len != 5) {
		goto err;
	}

	if (!atoi_byte(cmd_words_b[1])) {
		goto err;
	}
	sds_id = atoi_results_u8;

	hex_size = atoi_hex(cmd_words_b[2]);
	if (hex_size != 1) {
		goto err;
	}
	page = hexvalue[0];

	hex_size = atoi_hex(cmd_words_b[3]);
	if (hex_size != 1) {
		goto err;
	}
	reg = hexvalue[0];

	hex_size = atoi_hex(cmd_words_b[4]);
	if (hex_size == 0 || hex_size > 2) {
		goto err;
	}
	val = hexvalue[0];
	if (hex_size == 2) {
		val <<= 8;
		val |= hexvalue[1];
	}

	print_string("SDSSET: ");
	itoa(sds_id);
	print_string(":0x");
	print_byte(page);
	print_string(":0x");
	print_byte(reg);
	print_string(": VAL: ");

	sds_write_v(sds_id, page, reg, val);

	print_short(val);
	write_char('\n');
	return;

err:
	cmd_error("sdsset <sds-id> <hex:page> <hex:reg> <hex:val>\n");
	return;
}

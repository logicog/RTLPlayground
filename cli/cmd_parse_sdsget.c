/*
 * "sdsget" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_sdsget.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_sdsget(void)
{
	__xdata uint8_t sds_id, page, reg, hex_size;

	if (cmd_words_len != 4) {
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

	print_string("SDSGET: ");
	itoa(sds_id);
	print_string(":0x");
	print_byte(page);
	print_string(":0x");
	print_byte(reg);
	print_string(": VAL: ");

	sds_read(sds_id, page, reg);
	print_phy_data();
	write_char('\n');
	return;

err:
	cmd_error("sdsget <sds-id> <hex:page> <hex:reg>\n");
	return;
}

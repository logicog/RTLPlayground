/*
 * "sfp" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_sfp.h"
#include "sfp.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __code const struct machine machine;

bool sfp_print_measurements(uint8_t sfp)
{
	if (!sfp_read_block(sfp, 92, 1))
		return false;

	print_string("Options: "); print_byte(sfp_buf[0]); write_char('\n');
	if (!(sfp_options[sfp] & 0x40))
		return true;
	if (!sfp_read_block(sfp, 224, 16))
		return false;
	print_string("Temp: "); print_byte(sfp_buf[0]); print_byte(sfp_buf[1]); write_char('\n');
	print_string("Vcc: "); print_byte(sfp_buf[2]); print_byte(sfp_buf[3]); write_char('\n');
	print_string("TX Bias: "); print_byte(sfp_buf[4]); print_byte(sfp_buf[5]); write_char('\n');
	print_string("TX Power: "); print_byte(sfp_buf[6]); print_byte(sfp_buf[7]); write_char('\n');
	print_string("RX Power: "); print_byte(sfp_buf[8]); print_byte(sfp_buf[9]); write_char('\n');
	print_string("Laser: "); print_byte(sfp_buf[10]); print_byte(sfp_buf[11]); write_char('\n');
	print_string("State: "); print_byte(sfp_buf[14]); write_char('\n');

	return true;
}

void cmd_parse_sfp(void)
{
	uint8_t slot;

	if (cmd_words_len != 1 && cmd_words_len != 3)
		goto err;

	if (cmd_words_len == 1) {
		for (slot = 0; slot < machine.n_sfp; slot++) {
			print_string("\nSlot "); write_char('1' + slot);
			if (gpio_pin_test(machine.sfp_port[slot].pin_detect)) {
				print_string(" - empty\n");
				continue;
			}
			if (!sfp_read_block(slot, 11, 2)) {
				print_string(" - I2C read failed on this slot\n");
				continue;
			}
			print_string(" - Rate: "); print_byte(sfp_buf[1]);
			print_string("  Encoding: "); print_byte(sfp_buf[0]);
			write_char('\n');
			if (!sfp_print_info(slot) || !sfp_print_measurements(slot))
				print_string("I2C read failed on this slot\n");
		}
		return;
	}
	uint8_t idx = cmd_words_b[1];
	uint8_t ret = atoi_byte(idx);
	idx += ret;
	slot = atoi_results_u8 - 1;
	if (ret == 0 || !cmd_is_space(idx) || slot > 1) {
		cmd_error("Illegal SFP slot number\n");
		return;
	}
	if (slot >= machine.n_sfp) {
		cmd_error("SFP slot not present\n");
		return;
	}

	if (cmd_compare(2, "10g")) {
		print_string(" 10G\n");
		sfp_speed[slot] = SFP_SPEED_10G;
	} else if (cmd_compare(2, "2g5")) {
		print_string(" 2.5G\n");
		sfp_speed[slot] = SFP_SPEED_2G5;
	} else if (cmd_compare(2, "1g")) {
		print_string(" 1G\n");
		sfp_speed[slot] = SFP_SPEED_1G;
	} else if (cmd_compare(2, "100m")) {
		print_string(" 100M\n");
		sfp_speed[slot] = SFP_SPEED_100M;
	} else if (cmd_compare(2, "auto")) {
		print_string(" AUTO\n");
		sfp_speed[slot] = SFP_SPEED_AUTO;
	} else {
		goto err;
	}
	sfp_pins_last |= 0x1 << (slot << 2);
	handle_sfp();
	return;
err:
	cmd_error("\nUsage:\n\tsfp\n\tsfp [1|2] [1g|2g5|10g]\n");
}

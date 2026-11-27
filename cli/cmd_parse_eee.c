/*
 * "eee" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_eee.h"
#include "rtl837x_port.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __code const struct machine machine;

static inline uint8_t isnumber(uint8_t l)
{
	// return (l >= '0' && l <= '9');
	l -= '0';
	return (l <= ('9'-'0'));
}

void cmd_parse_eee(void)
{
	__xdata int8_t port = -1;
	__xdata uint8_t speed = EEE_2G5;
	__xdata uint8_t speed_word = 0;

	if (machine.n_10g)
		speed = EEE_10G;
	// Check if word 2 is a speed (contains 'g' or 'm') or a port number
	if (cmd_words_len >= 3) {
		uint8_t idx = cmd_words_b[2];
		// Skip digits to check if there's a letter after
		while (isnumber(cmd_buffer[idx]))
			idx++;
		if (cmd_buffer[idx] == 'g' || cmd_buffer[idx] == 'm') {
			// Word 2 is a speed (e.g., "2g5", "100m", "1g")
			speed_word = 2;
		} else if (cmd_is_space_or_nul(idx)) {
			// Word 2 is a port number
			if (cmd_arg_port_separator(cmd_words_b[2]) == 0) {
				cmd_error("Speed word invalid, use: [100m|1g|2g5]\n");
				return;
			}
			port = atoi_results_u8;
			// Check if word 3 is a speed
			if (cmd_words_len >= 4)
				speed_word = 3;
		}
	}
	// Parse speed if found
	if (speed_word > 0) {
		if (cmd_compare(speed_word, "100m"))
			speed = EEE_100;
		else if (cmd_compare(speed_word, "1g"))
			speed = EEE_1000;
		else if (cmd_compare(speed_word, "2g5"))
			speed = EEE_2G5;
		else 
		{
			cmd_error("Speed word invalid, use: [100m|1g|2g5]\n");
			return;
		}
	}
	if (cmd_compare(1, "on")) {
		if (port >= 0)
			port_eee_enable(port, speed);
		else
			port_eee_enable_all(speed);
	} else if (cmd_compare(1, "off")) {
		if (port >= 0)
			port_eee_disable(port);
		else
			port_eee_disable_all();
	} else if (cmd_compare(1, "status")) {
		if (port >= 0)
			port_eee_status(port);
		else
			port_eee_status_all();
	} else {
		cmd_error("eee [on|off|status] [port] [100m|1g|2g5]\n");
	}
}

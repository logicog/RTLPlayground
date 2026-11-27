/*
 * "lag" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_lag.h"
#include "rtl837x_port.h"
#include "rtl837x_lacp.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern volatile __xdata uint8_t sfr_data[4];

void cmd_parse_lag(void)
{
	__xdata uint8_t group;
	__xdata uint16_t members = 0;

	if (cmd_compare(1, "show")) {
		print_string("LAG status:\n");
		for (uint8_t i = 0; i < 4; i++) {
			write_char(' '); write_char('1' + i);
			members = port_lag_members_get(i);
			if (!members) {
				print_string(" disabled\n");
				continue;
			}
			print_string(" member ports: ");
			for (uint8_t j = 0; j < 10; j++) {
				if (members & 1) {
					print_phys_port(j);
					write_char(' ');
				}
				members >>= 1;
			}
			print_string(" (hash: 0x"); 
			reg_read_m(RTL837X_TRK_HASH_CTRL_BASE + (i << 2));
			print_byte(sfr_data[3]);
			print_string(")\n");
		}
		return;
	}

	if (cmd_words_len < 3)
		goto err;

	// Parse group, expect only one number 0-9;
	if (atoi_byte(cmd_words_b[1]) != 1)
		goto err;

	group = atoi_results_u8 - 1;
	if (group > 3)
		goto err;

	if (cmd_compare(2, "d")) {
		lacp_lag_set(group, 0);
		port_lag_members_set(group, 0);
		return;
	}

	uint8_t w = 2;
	__xdata uint8_t lacp_mode = 0;
	if (cmd_words_len > 2 && cmd_compare(2, "lacp")) {
		lacp_mode = 1;
		w = 3;
		if (cmd_words_len > 3 && cmd_compare(3, "off")) {
			lacp_lag_set(group, 0);
			return;
		}
	}
	while (w < cmd_words_len) {
		// Parse port.
		if (cmd_arg_port_separator(cmd_words_b[w++]) == 0)
			goto err;

		members |= ((uint16_t)1) << atoi_results_u8;
	}
	if (lacp_mode) {
		lacp_lag_set(group, members);
	} else {
		lacp_lag_set(group, 0);
		port_lag_members_set(group, members);
	}
	return;
err:
	cmd_error("lag (show | <1-4> (d | [lacp] <port>...))\n");
}

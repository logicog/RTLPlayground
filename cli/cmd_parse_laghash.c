/*
 * "laghash" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_laghash.h"
#include "rtl837x_port.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_laghash(void)
{
	// Parse group, expect only one number 0-9.
	if (cmd_words_len < 3 || atoi_byte(cmd_words_b[1]) != 1)
		goto err;

	uint8_t group = atoi_results_u8 - 1;
	if (group > 3)
		goto err;

	uint8_t hash = 0;
	uint8_t w = 2;
	while (w < cmd_words_len) {
		if (cmd_compare(w, "spa"))
			hash |= LAG_HASH_SOURCE_PORT_NUMBER;
		else if (cmd_compare(w, "smac"))
			hash |= LAG_HASH_L2_SMAC;
		else if (cmd_compare(w, "dmac"))
			hash |= LAG_HASH_L2_DMAC;
		else if (cmd_compare(w, "sip"))
			hash |= LAG_HASH_L3_SIP;
		else if (cmd_compare(w, "dip"))
			hash |= LAG_HASH_L3_DIP;
		else if (cmd_compare(w, "sport"))
			hash |= LAG_HASH_L4_SPORT;
		else if (cmd_compare(w, "dport"))
			hash |= LAG_HASH_L4_DPORT;
		else {
			print_string("Error: invalid hash type:");
			print_string_x(&cmd_buffer[cmd_words_b[w]]);
			write_char('\n');
			goto err;
		}
		w++;
	}
	port_lag_hash_set(group, hash);
	return;
err:
	cmd_error("laghash <1-4> [smac|dmac|sip|dip|sport|dport]\n");
}

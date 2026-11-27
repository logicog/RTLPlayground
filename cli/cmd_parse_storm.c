/*
 * "storm" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_storm.h"
#include "rtl837x_storm.h"

#pragma codeseg BANK2
#pragma constseg BANK2

static __code const char * __code const storm_types[STORM_TYPES] = { "bcast", "mcast", "ucast", "umcast" };

void cmd_parse_storm(void)
{
	uint8_t port, type, d;
	__xdata uint32_t rate;
	__xdata uint8_t *p;

	if (cmd_words_len == 1)
		return storm_show();
	if (cmd_words_len < 4 || !cmd_arg_port_separator(cmd_words_b[1]))
		goto err;
	port = atoi_results_u8;

	type = 0;
	while (!cmd_compare(2, storm_types[type])) {
		if (++type == STORM_TYPES)
			goto err;
	}

	if (cmd_words_len == 4 && cmd_compare(3, "off"))
		return storm_off(port, type);
	if (cmd_words_len != 5)
		goto err;

	rate = 0;
	p = &cmd_buffer[cmd_words_b[3]];
	do {
		d = *p++ - '0';
		if (d > 9 || rate > 99999999)
			goto err;
		rate = (rate << 3) + (rate << 1) + d;
	} while (*p != ' ' && *p != NUL);

	if (cmd_compare(4, "pps")) {
		if (!rate || rate > 0xfffff)
			goto err;
		storm_set(port, type, rate, 1);
	} else if (cmd_compare(4, "kbps")) {
		if (!rate || rate > 10000000)
			goto err;
		storm_set(port, type, rate, 0);
	} else {
		goto err;
	}
	return;

err:
	cmd_error("usage: storm [<port> bcast|mcast|ucast|umcast <rate> pps|kbps|off]\n");
}

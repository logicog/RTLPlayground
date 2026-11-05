/*
 * Command line interface of the sFlow agent for the RTL837x platform
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "cmd_parser.h"
#include "sflow.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void sflow_parse(void) __banked __reentrant
{
	if (cmd_words_len < 2) {
		print_string(sflow_state.conn ? "sFlow on, collector " : sflow_state.enabled ? "sFlow waiting, collector " : "sFlow off, collector ");
		print_ip(sflow_state.collector);
		write_char(':');
		itoa_short(sflow_state.port);
		print_string(", interval ");
		itoa_short(sflow_state.interval);
		print_string(" s, datagrams ");
		print_long(sflow_state.seq);
		print_string(", sampling ");
		itoa_short(sflow_state.rate);
		print_string(", drops ");
		print_long(sflow_state.drops);
		write_char('\n');
		return;
	}
	if (cmd_compare(1, "on")) {
		sflow_start();
		if (!sflow_state.collector[0])
			print_string("sFlow: set a collector to start sending\n");
		return;
	}
	if (cmd_compare(1, "off")) {
		sflow_state.enabled = false;
		sflow_stop();
		return;
	}
	if (cmd_compare(1, "collector") && cmd_words_len >= 3 && parse_ip(cmd_words_b[2])) {
		if (cmd_words_len >= 4 && (!atoi_short(cmd_words_b[3]) || !atoi_results_short))
			goto err;
		sflow_stop();
		memcpy(sflow_state.collector, ip, 4);
		sflow_state.port = cmd_words_len >= 4 ? atoi_results_short : SFLOW_PORT_DEFAULT;
		if (sflow_state.enabled)
			sflow_start();
		return;
	}
	if (cmd_compare(1, "interval") && cmd_words_len >= 3 && atoi_short(cmd_words_b[2])
	    && atoi_results_short && atoi_results_short <= 3600) {
		sflow_interval(atoi_results_short);
		return;
	}
	if (cmd_compare(1, "sampling") && cmd_words_len >= 3 && atoi_short(cmd_words_b[2])
	    && (!atoi_results_short || atoi_results_short >= SFLOW_RATE_MIN)) {
		if (sflow_rate(atoi_results_short))
			return;
		err_status = ERR_INVALID_ARGUMENT;
		print_string("Error: the mirror session is in use, run mirror off first\n");
		return;
	}
err:
	err_status = ERR_INVALID_ARGUMENT;
	print_string("Error: sflow [on|off|collector <ip> [port]|interval <1-3600>|sampling <0|100-65535>]\n");
}

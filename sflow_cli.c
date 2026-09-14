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

extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[15];
extern __xdata uint8_t ip[4];
extern __xdata uint16_t atoi_results_short;
uint8_t cmd_compare(uint8_t start, __code uint8_t * cmd);
uint8_t atoi_short(uint8_t idx);
uint8_t parse_ip(uint8_t idx);


void sflow_parse(void) __banked __reentrant
{
	if (cmd_words_len < 2) {
		print_string(sflow_state.enabled ? "sFlow on, collector " : "sFlow off, collector ");
		print_ip(sflow_state.collector);
		write_char(':');
		itoa_short(sflow_state.port);
		print_string(", interval ");
		itoa_short(sflow_state.interval);
		print_string(" s, datagrams ");
		print_long(sflow_state.seq);
		write_char('\n');
		return;
	}
	if (cmd_compare(1, "on")) {
		sflow_start();
		if (!sflow_state.conn)
			print_string("sFlow: set a collector to start sending\n");
		return;
	}
	if (cmd_compare(1, "off")) {
		sflow_state.enabled = 0;
		sflow_stop();
		return;
	}
	if (cmd_compare(1, "collector") && cmd_words_len >= 3 && parse_ip(cmd_words_b[2])) {
		if (cmd_words_len >= 4 && (!atoi_short(cmd_words_b[3]) || !atoi_results_short))
			goto err;
		sflow_stop();
		sflow_state.collector[0] = ip[0];
		sflow_state.collector[1] = ip[1];
		sflow_state.collector[2] = ip[2];
		sflow_state.collector[3] = ip[3];
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
err:
	print_string("Error: sflow [on|off|collector <ip> [port]|interval <1-3600>]\n");
}

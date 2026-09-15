/*
 * Command line interface of the DNS resolver for the RTL837x platform
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "cmd_parser.h"
#include "dns.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata uint8_t cmd_buffer[CMD_BUF_SIZE];
extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[15];
extern __xdata uint8_t ip[4];
uint8_t cmd_compare(uint8_t start, __code uint8_t * cmd);
uint8_t parse_ip(uint8_t idx);


void dns_parse(void) __banked __reentrant
{
	__xdata uint8_t *s;
	uint8_t n;

	if (cmd_words_len < 2) {
		dns_show();
		return;
	}
	if (cmd_compare(1, "server") && cmd_words_len >= 3 && parse_ip(cmd_words_b[2])) {
		memcpy(dns_state.server[0], ip, 4);
		memset(dns_state.server[1], 0, 4);
		if (cmd_words_len >= 4) {
			if (!parse_ip(cmd_words_b[3]))
				goto err;
			memcpy(dns_state.server[1], ip, 4);
		}
		return;
	}
	if (cmd_compare(1, "lookup") && cmd_words_len >= 3) {
		if (dns_state.status == DNS_PENDING) {
			print_string("DNS: a lookup is already running\n");
			return;
		}
		s = &cmd_buffer[cmd_words_b[2]];
		for (n = 0; s[n] && s[n] != ' '; n++) {
			if (n == DNS_NAME_LEN - 1)
				goto err;
			dns_state.name[n] = s[n];
		}
		dns_state.name[n] = 0;
		dns_state.verbose = 1;
		dns_lookup();
		if (dns_state.status == DNS_DONE) {
			print_ip(dns_state.addr);
			write_char('\n');
		}
		return;
	}
err:
	print_string("Error: dns [server <ip> [<ip>]|lookup <name>]\n");
}

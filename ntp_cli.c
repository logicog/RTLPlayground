/*
 * Command line interface of the NTP client for the RTL837x platform
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "cmd_parser.h"
#include "ntp.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata uint8_t cmd_buffer[CMD_BUF_SIZE];
extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[15];
extern __xdata uint16_t atoi_results_short;
uint8_t cmd_compare(uint8_t start, __code const uint8_t * cmd);
uint8_t atoi_short(uint8_t idx);


static uint8_t ntp_cli_server(void) __reentrant
{
	__xdata uint8_t *s = &cmd_buffer[cmd_words_b[2]];
	uint8_t n;

	for (n = 0; s[n] && s[n] != ' '; n++)
		if (n == DNS_NAME_LEN - 1)
			return 0;
	memcpy(ntp_state.server, s, n);
	ntp_state.server[n] = 0;
	return n;
}


static uint8_t ntp_cli_timezone(void) __reentrant
{
	__xdata uint8_t *s = &cmd_buffer[cmd_words_b[2]];
	uint8_t neg = 0, h = 0, m = 0, n = 0;
	uint16_t v;

	if (*s == '+' || *s == '-')
		neg = *s++ == '-';
	while (*s >= '0' && *s <= '9') {
		h = h * 10 + (*s++ - '0');
		if (++n > 2)
			return 0;
	}
	if (!n)
		return 0;
	if (*s == ':') {
		s++;
		if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9')
			return 0;
		m = (s[0] - '0') * 10 + s[1] - '0';
		s += 2;
	}
	if ((*s && *s != ' ') || (m && m != 30 && m != 45))
		return 0;
	v = (uint16_t)h * 60 + m;
	if (v > (neg ? 720 : 840))
		return 0;
	ntp_state.offset = neg ? -(int16_t)v : (int16_t)v;
	return 1;
}


void ntp_parse(void) __banked __reentrant
{
	if (cmd_compare(0, "time")) {
		ntp_show_time();
		return;
	}
	if (cmd_words_len < 2) {
		ntp_show();
		return;
	}
	if (cmd_compare(1, "on")) {
		ntp_start();
		return;
	}
	if (cmd_compare(1, "off")) {
		ntp_stop();
		return;
	}
	if (cmd_words_len < 3)
		goto err;
	if (cmd_compare(1, "server")) {
		if (!ntp_cli_server())
			goto err;
		if (ntp_state.enabled)
			ntp_start();
		return;
	}
	if (cmd_compare(1, "interval")) {
		if (!atoi_short(cmd_words_b[2]) || !atoi_results_short || atoi_results_short > 1440)
			goto err;
		ntp_state.interval = atoi_results_short;
		return;
	}
	if (cmd_compare(1, "timezone")) {
		if (!ntp_cli_timezone())
			goto err;
		return;
	}
	if (cmd_compare(1, "dst")) {
		if (cmd_compare(2, "off"))
			ntp_state.dst = NTP_DST_OFF;
		else if (cmd_compare(2, "eu"))
			ntp_state.dst = NTP_DST_EU;
		else if (cmd_compare(2, "us"))
			ntp_state.dst = NTP_DST_US;
		else
			goto err;
		return;
	}
err:
	print_string("Error: ntp [on|off|server <name>|interval <1-1440>|timezone <+hh:mm>|dst off|eu|us]\n");
}

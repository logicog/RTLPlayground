/* Main-loop-owned SNMP configuration. No Flash writes or large buffers here. */
#include "rtl837x_common.h"
#include "cmd_parser.h"
#include "uip/uip.h"
#include "snmp.h"
#include "snmp_internal.h"
#pragma codeseg BANK2
#pragma constseg BANK2
extern __xdata uint8_t cmd_words_len, cmd_words_b[], save_cmd;
extern uint8_t cmd_compare(uint8_t start, __code const uint8_t *cmd);
extern __xdata uint8_t outbuf[];
extern __xdata uint16_t slen;
static __xdata uint8_t output_http;
static void emit(__xdata uint8_t c)
{
	if (output_http) {
		if (slen < TCP_OUTBUF_SIZE)
			outbuf[slen++] = c;
	} else {
		write_char(c);
	}
}
static void text(__code const char *__xdata p)
{
	while (*p)
		emit(*p++);
}
/* Only octets and prefix lengths are formatted, never a wide epoch value. */
static void number(__xdata uint8_t value)
{
	if (value >= 100)
		emit('0' + value / 100);
	if (value >= 10)
		emit('0' + value / 10 % 10);
	emit('0' + value % 10);
}
bool snmp_set_enabled(__xdata uint8_t enabled) __banked
{
	if (enabled > 1 || (enabled && (!snmp_listener || snmp_listener->lport != HTONS(161) ||
					!snmp_community_length || !snmp_mask[0])))
		return false;
	snmp_agent_enabled = enabled;
	return true;
}
void snmp_get_status(__xdata uint8_t *__xdata status) __banked
{
	uint8_t i;
	status[0] = snmp_agent_enabled && snmp_listener != 0 &&
		    snmp_listener->lport == HTONS(161) && snmp_community_length != 0;
	status[1] = snmp_community_length != 0;
	for (i = 0; i < 4; i++) {
		status[2 + i] = snmp_community_length ? snmp_network[i] : 0;
		status[6 + i] = snmp_community_length ? snmp_mask[i] : 0;
	}
}
bool snmp_set_community(__xdata const uint8_t *__xdata c, __xdata uint8_t len) __banked
{
	__xdata uint8_t i;
	__xdata struct uip_udp_conn *__xdata candidate;
	if (!c || !len || len > 32 || (snmp_listener && snmp_listener->lport != HTONS(161)))
		return false;
	for (i = 0; i < len; i++)
		if (c[i] < 33 || c[i] > 126)
			return false;
	if (len == snmp_community_length && !memcmp(c, snmp_community, len))
		return true;
	if (!snmp_listener) {
		candidate = uip_udp_new(0, 0);
		if (!candidate)
			return false;
		uip_udp_bind(candidate, HTONS(161));
		snmp_listener = candidate;
	}
	memcpy(snmp_community, c, len);
	snmp_community_length = len;
	for (i = len; i < 32; i++)
		snmp_community[i] = 0;
	return true;
}
bool snmp_set_manager(__xdata const uint8_t *__xdata s, __xdata uint8_t len) __banked
{
	__xdata uint8_t a[4], m[4];
	/* SDCC 4.5.0 #15242 caches plain pos/value in three DATA bytes. */
	__xdata volatile uint8_t pos = 0;
	__xdata uint8_t i, digits, prefix, bits;
	__xdata volatile uint16_t value;
	if (!s || len < 9 || len > 18)
		return false;
	for (i = 0; i < 4; i++) {
		value = 0;
		digits = 0;
		while (pos < len && s[pos] >= '0' && s[pos] <= '9') {
			if (++digits > 3)
				return false;
			value = value * 10 + (s[pos++] - '0');
		}
		if (!digits || value > 255 || pos >= len || s[pos++] != (i == 3 ? '/' : '.'))
			return false;
		a[i] = value;
	}
	value = 0;
	digits = 0;
	while (pos < len && s[pos] >= '0' && s[pos] <= '9') {
		if (++digits > 2)
			return false;
		value = value * 10 + (s[pos++] - '0');
	}
	if (!digits || pos != len || !value || value > 32 || !a[0] || a[0] == 127 || a[0] >= 224)
		return false;
	prefix = value;
	bits = prefix;
	for (i = 0; i < 4; i++) {
		m[i] = bits >= 8 ? 255 : (uint8_t)(255u << (8 - bits));
		bits = bits >= 8 ? bits - 8 : 0;
		a[i] &= m[i];
	}
	if (!a[0] || a[0] == 127 || a[0] >= 224)
		return false;
	memcpy(snmp_network, a, 4);
	memcpy(snmp_mask, m, 4);
	return true;
}
static uint8_t prefix(void)
{
	__xdata uint8_t i, b, n = 0;
	for (i = 0; i < 4; i++)
		for (b = 128; b; b >>= 1)
			if (snmp_mask[i] & b)
				n++;
	return n;
}
static void manager(void)
{
	__xdata uint8_t i;
	for (i = 0; i < 4; i++) {
		if (i)
			emit('.');
		number(snmp_network[i]);
	}
	emit('/');
	number(prefix());
}
void snmp_show(void) __banked
{
	output_http = 0;
	text("SNMP Agent: ");
	text(snmp_agent_enabled && snmp_listener && snmp_community_length ? "Enabled" : "Disabled");
	text("\nProtocol: SNMPv2c\nAccess: Read-only\nCommunity: ");
	text(snmp_community_length ? "Configured" : "Not configured");
	text("\nManager: ");
	if (snmp_mask[0])
		manager();
	else
		text("Unknown");
	text("\n");
}
void snmp_export_config(void) __banked
{
	__xdata uint8_t i;
	/* Legacy binary communities cannot safely round-trip through startup text. */
	for (i = 0; i < snmp_community_length; i++)
		if (snmp_community[i] < 33 || snmp_community[i] > 126) {
			slen =
			    strtox(outbuf, "HTTP/1.1 400 Bad Request\r\nConnection: "
					   "close\r\nCache-Control: no-store\r\n\r\nSNMP "
					   "configuration is not representable in startup text.\n");
			return;
		}
	slen = strtox(outbuf, "HTTP/1.1 200 OK\r\nConnection: close\r\nCache-Control: "
			      "no-store\r\nContent-Type: text/plain\r\n\r\n");
	output_http = 1;
	if (snmp_mask[0]) {
		text("snmp manager ");
		manager();
		emit('\n');
	}
	if (snmp_community_length) {
		text("snmp community ");
		for (i = 0; i < snmp_community_length; i++)
			emit(snmp_community[i]);
		emit('\n');
	}
	text(snmp_agent_enabled ? "snmp on\n" : "snmp off\n");
}
bool snmp_command(void) __banked
{
	__xdata const uint8_t *__xdata p;
	__xdata uint8_t len = 0;
	if (cmd_words_len == 2) {
		if (cmd_compare(1, "show")) {
			snmp_show();
			return true;
		}
		if (cmd_compare(1, "on") || cmd_compare(1, "off"))
			return snmp_set_enabled(cmd_compare(1, "on"));
	}
	if (cmd_words_len != 3)
		return false;
	p = cmd_buffer + cmd_words_b[2];
	while (len < CMD_BUF_SIZE - cmd_words_b[2] && p[len] && p[len] != ' ')
		len++;
	if (cmd_compare(1, "community"))
		return snmp_set_community(p, len);
	if (cmd_compare(1, "manager"))
		return snmp_set_manager(p, len);
	/* Ignore old timestamp metadata during startup migration only. */
	if (!save_cmd && cmd_compare(1, "changed"))
		return true;
	return false;
}

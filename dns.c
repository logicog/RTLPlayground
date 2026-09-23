/*
 * DNS resolver for the RTL837x platform
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "cmd_parser.h"
#include "uip/uip.h"
#include "dhcp.h"
#include "dns.h"

#pragma codeseg BANK3
#pragma constseg BANK3

#define DNS_PORT	53
#define DNS_TRIES	6
#define DNS_TIMEOUT	(2 * SYS_TICK_HZ)

extern __xdata struct dhcp_state dhcp_state;
extern volatile __xdata uint32_t ticks;

__xdata struct dns_state dns_state;
__xdata uip_ipaddr_t dns_ip;
uint8_t * __xdata dns_cand[3];


static uint8_t dns_used(__xdata uint8_t *a) __reentrant
{
	return a[0] | a[1] | a[2] | a[3];
}


static uint8_t dns_name_is_ip(void) __reentrant
{
	__xdata char *s = dns_state.name;
	uint8_t i, n;
	uint16_t v;

	for (i = 0; i < 4; i++) {
		v = 0;
		n = 0;
		while (*s >= '0' && *s <= '9') {
			v = v * 10 + (*s++ - '0');
			if (++n > 3 || v > 255)
				return 0;
		}
		if (!n)
			return 0;
		dns_state.addr[i] = v;
		if (i < 3 && *s++ != '.')
			return 0;
	}
	return !*s;
}


static void dns_next(void) __reentrant
{
	uint8_t k = 0;

	if (dns_state.conn) {
		uip_udp_remove(dns_state.conn);
		dns_state.conn = 0;
	}
	if (dns_used(dns_state.server[0]))
		dns_cand[k++] = dns_state.server[0];
	if (dns_used(dns_state.server[1]))
		dns_cand[k++] = dns_state.server[1];
	if (dns_used(dhcp_state.dns))
		dns_cand[k++] = dhcp_state.dns;
	if (!k || dns_state.tries >= DNS_TRIES) {
		dns_state.status = DNS_FAILED;
		if (dns_state.verbose)
			print_string(k ? "DNS: no answer\n" : "DNS: no server\n");
		return;
	}
	k = dns_state.tries % k;
	uip_ipaddr(dns_ip, dns_cand[k][0], dns_cand[k][1], dns_cand[k][2], dns_cand[k][3]);
	dns_state.conn = uip_udp_new(&dns_ip, HTONS(DNS_PORT));
	if (!dns_state.conn) {
		dns_state.status = DNS_FAILED;
		print_string("DNS: no free UDP connection\n");
		return;
	}
	dns_state.send = 1;
}


static void dns_query(void) __reentrant
{
	__xdata uint8_t *p = (__xdata uint8_t *)uip_appdata;
	__xdata char *n = dns_state.name;
	__xdata uint8_t *label;
	uint16_t len = 12;

	dns_state.id++;
	p[0] = dns_state.id >> 8;
	p[1] = dns_state.id;
	p[2] = 0x01;
	p[3] = 0x00;
	p[4] = 0;
	p[5] = 1;
	p[6] = p[7] = p[8] = p[9] = p[10] = p[11] = 0;
	while (*n) {
		label = p + len++;
		*label = 0;
		while (*n && *n != '.') {
			p[len++] = *n++;
			(*label)++;
		}
		if (*n)
			n++;
	}
	p[len++] = 0;
	p[len++] = 0;
	p[len++] = 1;
	p[len++] = 0;
	p[len++] = 1;
	uip_udp_send(len);
}


static uint16_t dns_skip_name(__xdata uint8_t *p, uint16_t i, uint16_t len) __reentrant
{
	while (i < len) {
		if (!p[i])
			return i + 1;
		if ((p[i] & 0xc0) == 0xc0)
			return i + 2;
		i += p[i] + 1;
	}
	return len;
}


static void dns_answer(void) __reentrant
{
	__xdata uint8_t *p = (__xdata uint8_t *)uip_appdata;
	uint16_t len = uip_datalen();
	uint16_t i, an;

	if (dns_state.status != DNS_PENDING || len < 12 || !(p[2] & 0x80)
	    || p[0] != (uint8_t)(dns_state.id >> 8) || p[1] != (uint8_t)dns_state.id)
		return;
	an = ((uint16_t)p[6] << 8) | p[7];
	i = dns_skip_name(p, 12, len) + 4;
	while (!(p[3] & 0x0f) && an-- && i + 10 <= len) {
		i = dns_skip_name(p, i, len);
		if (i + 10 > len)
			break;
		if (!p[i] && p[i + 1] == 1 && !p[i + 8] && p[i + 9] == 4 && i + 14 <= len) {
			memcpy(dns_state.addr, p + i + 10, 4);
			dns_state.status = DNS_DONE;
			uip_udp_remove(dns_state.conn);
			dns_state.conn = 0;
			if (dns_state.verbose) {
				print_string("DNS: ");
				print_string_x(dns_state.name);
				print_string(" is ");
				print_ip(dns_state.addr);
				write_char('\n');
			}
			return;
		}
		i += 10 + (((uint16_t)p[i + 8] << 8) | p[i + 9]);
	}
	dns_state.tries = DNS_TRIES;
	dns_next();
}


void dns_init(void) __banked
{
	memset(dns_state.server, 0, sizeof(dns_state.server));
	dns_state.name[0] = 0;
	dns_state.status = DNS_IDLE;
	dns_state.conn = 0;
}


void dns_lookup(void) __banked
{
	if (dns_name_is_ip()) {
		dns_state.status = DNS_DONE;
		return;
	}
	dns_state.tries = 0;
	dns_state.status = DNS_PENDING;
	dns_next();
}


void dns_show(void) __banked
{
	print_string("DNS servers ");
	print_ip(dns_state.server[0]);
	write_char(' ');
	print_ip(dns_state.server[1]);
	print_string(", from DHCP ");
	print_ip(dhcp_state.dns);
	write_char('\n');
}


void dns_callback(uint16_t lport) __banked __reentrant
{
	if (!dns_state.conn || lport != dns_state.conn->lport)
		return;
	if (uip_newdata()) {
		dns_answer();
		return;
	}
	if (dns_state.status != DNS_PENDING)
		return;
	if (dns_state.send) {
		dns_state.send = 0;
		dns_state.sent = ticks;
		dns_query();
		return;
	}
	if (ticks - dns_state.sent >= DNS_TIMEOUT) {
		dns_state.tries++;
		dns_next();
	}
}

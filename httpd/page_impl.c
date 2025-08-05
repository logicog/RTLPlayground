#include "../rtl837x_common.h"
#include "uip.h"
#include "../html_data.h"

#pragma codeseg BANK1

extern __xdata uint8_t outbuf[2048];
extern __xdata uint16_t slen;
extern __code uint8_t * __code hex;
extern __xdata uip_ipaddr_t uip_hostaddr, uip_draddr, uip_netmask;
extern __code uint8_t ownMAC[];

inline void byte_to_html(uint8_t a)
{
	outbuf[slen++] = hex[(a >> 4) & 0xf];
	outbuf[slen++] = hex[a & 0xf];
}


inline void char_to_html(char c)
{
	outbuf[slen++] = c;
}


inline void itoa_html(uint8_t v)
{
	uint8_t t = (v / 100) % 10;
	if (t)
		char_to_html('0' + t);
	t = (v / 10) % 10;
	if (t)
		char_to_html('0' + t);
	char_to_html('0' + (v % 10));
}


uint16_t stat_content(void)
{
	print_string("stat_content called\n");
	return 0;
}


uint16_t port_status(void)
{
	print_string("port_status called\n");
	return 0;
}


uint16_t html_index(void)
{
	print_string("html_index called\n");
	slen += strtox(outbuf + slen, "<tr><td>IP Address</td><td>");
	print_string("\nIP:");
	print_byte(uip_hostaddr[0]); write_char('.');
	print_byte(uip_hostaddr[0] >> 8); write_char('.');
	print_byte(uip_hostaddr[1]); write_char('.');
	print_byte(uip_hostaddr[1] >> 8); write_char('\n');
	itoa_html(uip_hostaddr[0]); char_to_html('.');
	itoa_html(uip_hostaddr[0] >> 8); char_to_html('.');
	itoa_html(uip_hostaddr[1]); char_to_html('.');
	itoa_html(uip_hostaddr[1]);
	slen += strtox(outbuf + slen, "</td></tr><tr><td>Gateway</td><td>");
	itoa_html(uip_draddr[0]); char_to_html('.');
	itoa_html(uip_draddr[0] >> 8); char_to_html('.');
	itoa_html(uip_draddr[1]); char_to_html('.');
	itoa_html(uip_draddr[1] >> 8);
	slen += strtox(outbuf + slen, "</td></tr><tr><td>Netmask</td><td>");
	itoa_html(uip_netmask[0]); char_to_html('.');
	itoa_html(uip_netmask[0] >> 8); char_to_html('.');
	itoa_html(uip_netmask[1]); char_to_html('.');
	itoa_html(uip_netmask[1] >> 8);
	slen += strtox(outbuf + slen, "</td></tr><tr><td>MAC Address</td><td>");
	byte_to_html(ownMAC[0]); char_to_html(':');
	byte_to_html(ownMAC[1]); char_to_html(':');
	byte_to_html(ownMAC[2]); char_to_html(':');
	byte_to_html(ownMAC[3]); char_to_html(':');
	byte_to_html(ownMAC[4]); char_to_html(':');
	byte_to_html(ownMAC[5]);
	slen += strtox(outbuf + slen, "</td></tr>");
	return 0;
}

/*
 * "gw" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_gw.h"
#include "uip/uip.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_gw(void)
{
	if (cmd_words_len == 1) {
		print_string("Current gw: ");
		print_ip(uip_draddr); write_char('\n');
	} else {
		if (parse_ip(cmd_words_b[1]) != 0) {
			uip_ipaddr(&uip_draddr, ip[0], ip[1], ip[2], ip[3]);
			print_string("Setting gw: ");
			print_ip(ip); write_char('\n');
		} else {
			cmd_error("Invalid IP address\n" \
						 "Error: gw <ip-address>\n");
		}
	}
}

/*
 * "netmask" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_netmask.h"
#include "uip/uip.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_netmask(void)
{
	if (cmd_words_len == 1) {
		print_string("Current netmask: ");
		print_ip(uip_netmask); write_char('\n');
	} else {
		if (parse_ip(cmd_words_b[1]) != 0) {
			uip_ipaddr(&uip_netmask, ip[0], ip[1], ip[2], ip[3]);
			print_string("Setting netmask: ");
			print_ip(ip); write_char('\n');
		} else {
			cmd_error("Invalid IP address\n");
		}
	}
	write_char('\n');
}

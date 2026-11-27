/*
 * "ip" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_ip.h"
#include "uip/uip.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata struct dhcp_state dhcp_state;

void cmd_parse_ip(void)
{
	if (cmd_compare(1, "dhcp")) {
		dhcp_start();
	} else if (cmd_words_len == 1) {
		print_string("Current IP: ");
		print_ip(uip_hostaddr);
		if (dhcp_state.state == DHCP_LEASING) {
			print_string(" (dhcp, renewal in sec: ");
			print_short(dhcp_state.dhcp_timer);
			write_char(')');
		} else {
			print_string(" (static)");
		}
		write_char('\n');
	} else {
		if (parse_ip(cmd_words_b[1]) != 0) {
			if (dhcp_state.state)
				dhcp_stop();
			uip_ipaddr(&uip_hostaddr, ip[0], ip[1], ip[2], ip[3]);
			print_string("Setting ip: ");
			print_ip(ip); write_char('\n');
		} else {
			cmd_error("Invalid IP address\n" \
						 "Error: ip [<ip-address>|dhcp]\n" \
						 "  The dhcp option enables the dhcp client, calling ip without options prints the current IP\n" \
						 "  Calling with a valid IP address will stop any ongoing dhcp client and set the IP address\n");
		}
	}
}

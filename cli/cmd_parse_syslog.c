/*
 * "syslog" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_syslog.h"

#pragma codeseg BANK2
#pragma constseg BANK2

/* Scratch for cmd_parse_syslog(), in xdata: a local here would take internal RAM
 * the linker has none of. */
__xdata uint8_t  syslog_port_was_on;

void cmd_parse_syslog(void)
{
	if (cmd_words_len < 2) // no argument -> print status
	{
		print_string("Current syslog status: ");
		if (syslog_state.enabled) {
			print_string("enabled, sending to ");
			print_ip(syslog_state.server_ip);
			write_char(':'); itoa_short(syslog_state.server_port);
			write_char('\n');
		} else {
			print_string("disabled\n");
		}
		return;
	}

	if (cmd_compare(1, "on")) {
		syslog_start();
	} else if (cmd_compare(1, "off")){
		syslog_stop();
	} else if (cmd_compare(1, "ip")) {
		if (cmd_words_len < 3) { // no additional arguemnt -> print current ip
			print_string("Current syslog IP: ");
			print_ip(syslog_state.server_ip);
			return;
		} else if (parse_ip(cmd_words_b[2]) != 0) {
			uint8_t was_enabled = syslog_state.enabled;
			if (was_enabled)
				syslog_stop();
			print_string("Setting new syslog IP.\n");
			syslog_state.server_ip[0] = ip[0]; syslog_state.server_ip[1] = ip[1];
			syslog_state.server_ip[2] = ip[2]; syslog_state.server_ip[3] = ip[3];
			if (was_enabled)
				syslog_start();
		} else {
			cmd_error("Invalid IP address\n");
		}
	} else if (cmd_compare(1, "port")) {
		if (cmd_words_len < 3) { // no additional argument -> print current port
			print_string("Current syslog port: ");
			itoa_short(syslog_state.server_port); write_char('\n');
			return;
		}
		/* atoi_short() returns zero for no digits and for a value past
		 * 65535; port zero is not a port either, so both tests are needed. */
		if (!atoi_short(cmd_words_b[2]) || !atoi_results_short) {
			cmd_error("Invalid port\n");
			return;
		}
		/* The port is frozen into the connection by uip_udp_new(), so
		 * changing it has to tear the connection down and build a new one -
		 * same dance as setting the address above. */
		syslog_port_was_on = syslog_state.enabled;
		if (syslog_port_was_on)
			syslog_stop();
		print_string("Setting new syslog port.\n");
		syslog_state.server_port = atoi_results_short;
		if (syslog_port_was_on)
			syslog_start();
	}
	else
	{
		cmd_error("syslog [on|off|ip [ip-address]|port [number]]\n"
			  "  on/off enables or disables syslog, ip sets the syslog server IP address,\n"
			  "  port sets the destination UDP port (default 514)\n");
	}
}

/*
 * "port" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_port.h"
#include "rtl837x_port.h"
#include "rtl837x_phy.h"
#include "rtl837x_lldp.h"
#include "rtl837x_cabletest.h"
#include "machine.h"
#include "phy.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __code const struct machine machine;
extern __xdata bool lldp_enabled;

__xdata char port_names[9][PORT_NAME_SIZE];

void cmd_parse_port(void)
{
	if (cmd_words_len < 3) {
		cmd_error("\nUsage:" \
					 "\nport <port> [show|on|off]" \
					 "\nport <port> [10m|100m|1g|2g5|duplex] [half|full]" \
					 "\nport <port> lldp [block|permit]" \
					 "\nport <port> cabletest [show]" \
					 "\nport <port> name [custom port name]\n");
		return;
	}

	if (cmd_arg_port_separator(cmd_words_b[1]) == 0) {
		cmd_error("Invalid port number\n");
		return;
	}
	phy_settings.port = atoi_results_u8;

	print_string("Logical Port: "); print_byte(phy_settings.port); write_char('\n');
	phy_settings.duplex = PHY_DUPLEX_BOTH;

	if (cmd_compare(2, "show")) {
		print_string("Name: ");
		print_string_x(port_names[phy_settings.port]);
		if (!machine.is_sfp[phy_settings.port]) {
			phy_show(phy_settings.port);
		} else {
			write_char('\n');
		}
		port_media_show(phy_settings.port);
	} else if (cmd_compare(2, "name")) {
		uint8_t i = 0;
		while ( cmd_words_len > 3 && (i < PORT_NAME_SIZE-1) && (cmd_buffer[cmd_words_b[3] + i] != NUL) ) {
			port_names[phy_settings.port][i] = cmd_name_char(cmd_buffer[cmd_words_b[3] + i]);
			i++;
		}
		port_names[phy_settings.port][i] = NUL;
		print_string("\nName set to: \"");
		print_string_x(port_names[phy_settings.port]);
		print_string("\"\n");
	} else if (cmd_compare(2, "lldp")) {
		if (cmd_compare(3, "block")){
			if (lldp_enabled)
				lldp_shutdown((uint16_t)1 << phy_settings.port);
			lldp_logical_port_status &= ~((uint16_t)1 << phy_settings.port);
		} else if (cmd_compare(3, "permit")) {
			lldp_logical_port_status |= (uint16_t)1 << phy_settings.port;
		} else {
			cmd_error("Unknown port <port> lldp [block|permit] command\n");
		}
	} else if (machine.is_sfp[phy_settings.port]) {
		print_string(" is SFP no PHY information available.\n");
	} else if (cmd_compare(2, "cabletest")) {
		if (cmd_words_len > 3 && cmd_compare(3, "show"))
			cabletest_show();
		else
			cabletest_run();
	} else if (cmd_compare(2, "10m")) {
		print_string(" 10M\n");
		phy_settings.speed = PHY_SPEED_10M;
		if (cmd_compare(3, "half"))
			phy_settings.duplex = PHY_DUPLEX_HALF;
		else if (cmd_compare(3, "full"))
			phy_settings.duplex = PHY_DUPLEX_FULL;
		phy_set_speed();
	} else if (cmd_compare(2, "100m")) {
		print_string(" 100M\n");
		phy_settings.speed = PHY_SPEED_100M;
		if (cmd_compare(3, "half"))
			phy_settings.duplex = PHY_DUPLEX_HALF;
		else if (cmd_compare(3, "full"))
			phy_settings.duplex = PHY_DUPLEX_FULL;
		phy_set_speed();
	} else if (cmd_compare(2, "10g")) {
		print_string(" 10G\n");
		phy_settings.speed = PHY_SPEED_10G;
		phy_set_speed();
	} else if (cmd_compare(2, "5g")) {
		print_string(" 5G\n");
		phy_settings.speed = PHY_SPEED_5G;
		phy_set_speed();
	} else if (cmd_compare(2, "2g5")) {
		print_string(" 2.5G\n");
		phy_settings.speed = PHY_SPEED_2G5;
		phy_set_speed();
	} else if (cmd_compare(2, "1g")) {
		print_string(" 1G\n");
		phy_settings.speed = PHY_SPEED_1G;
		phy_set_speed();
	} else if (cmd_compare(2, "auto")) {
		print_string(" AUTO\n");
		phy_settings.speed = PHY_SPEED_AUTO;
		phy_set_speed();
	} else if (cmd_compare(2, "off")) {
		print_string(" OFF\n");
		phy_settings.speed = PHY_OFF;
		phy_set_speed();
	} else if (cmd_compare(2, "on")) {
		print_string(" ON\n");
		phy_settings.speed = PHY_SPEED_AUTO;
		phy_set_speed();
	} else if (cmd_compare(2, "duplex")) {
		print_string(" DUPLEX\n");
		if (cmd_compare(3, "full")) {
			phy_settings.duplex = PHY_DUPLEX_FULL;
		} else if (cmd_compare(3, "half")) {
			phy_settings.duplex = PHY_DUPLEX_HALF;
		} else {
			cmd_error("Usage: port <port> duplex full|half\n");
			return;
		}
		phy_set_duplex();
	} else {
		cmd_error("Unknown port command\n");
	}
}

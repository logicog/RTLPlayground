/*
 * "ingress" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_ingress.h"
#include "rtl837x_port.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

bool vlan_ingress_mode_parse(char c, __xdata vlan_ingress_mode_t *mode)
{
	switch (c) {
	case 'u':
		*mode = VLAN_UNTAGGED;
		return true;
	case 't':
		*mode = VLAN_TAGGED;
		return true;
	case 'a':
		*mode = VLAN_ALL;
		return true;
	default:
		*mode = VLAN_INVALID;
		return false;
	}
}

void cmd_parse_ingress(void)
{
	if (cmd_words_len < 2) {
		goto err;
	}
	uint8_t log_port = 0;
	__xdata vlan_ingress_mode_t mode = VLAN_INVALID;
	uint8_t idx = cmd_words_b[1];

	if (vlan_ingress_mode_parse(cmd_buffer[idx++], &mode)) {
		if (!cmd_is_space_or_nul(idx))
			goto err;
		// Setting mode for all ports at once
		for (log_port = MACHINE_MIN_PORT; log_port <= MACHINE_MAX_PORT; log_port++) {
			if (!port_ingress_filter(log_port, mode)) {
				cmd_error("cannot set the ingress filter for port "); print_phys_port(log_port); write_char('\n');
				return;
			}
		}
		print_string("All ports ingress filter set to: ");
		print_port_ingress_filter_mode(mode); write_char('\n');
	} else {
		for(uint8_t w = 1; w < cmd_words_len; w++) {
			idx = cmd_words_b[w];
			uint8_t ret = cmd_arg_port(idx);
			if (ret != 1) {
				cmd_error("Invalid physical port number\n");
				continue;
			}
			log_port = atoi_results_u8;
			idx += ret;

			if (!vlan_ingress_mode_parse(cmd_buffer[idx++], &mode) || !cmd_is_space_or_nul(idx)) {
				cmd_error("Invalid ingress mode for port "); print_phys_port(log_port); print_string(" in ingress command\n");
				goto err;
			}
			if (!port_ingress_filter(log_port, mode)) {
				cmd_error("cannot set the ingress filter for port "); print_phys_port(log_port); write_char('\n');
				return;
			}
			print_string("Port "); print_phys_port(log_port);
			print_string(" ingress filter set to: ");
			print_port_ingress_filter_mode(mode); write_char('\n');
		}
	}
	return;
err:
	cmd_error("ingress [p]<u/t/a>...\n");
}

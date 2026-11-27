/*
 * "vlan" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_vlan.h"
#include "rtl837x_port.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __code const uint8_t * __code const hex;
extern __xdata struct dhcp_state dhcp_state;
extern __xdata uint16_t management_vlan;

__xdata uint8_t vlan_names[VLAN_NAMES_SIZE];
__xdata uint16_t vlan_ptr;

static inline uint8_t isletter(uint8_t l)
{
	// return (l >= 'a' && l <= 'z') || (l >= 'A' && l <= 'Z');

	// Make it lowercase
	l |= 0x20;
	l -= 'a';
	return (l <= ('z'-'a'));
}

void cmd_parse_vlan(void)
{
	vlan_settings.vlan = 0;
	vlan_settings.members = 0;
	vlan_settings.tagged = 0;

	if (cmd_words_len < 2)
		goto err;

	// Parse the VLAN number
	if (atoi_short(cmd_words_b[1]) != 0) {
		if (atoi_results_short > 4094)
			goto err;
		vlan_settings.vlan = atoi_results_short;

		if (cmd_compare(2, "mgmt")) {
			if (management_vlan == vlan_settings.vlan) {
				print_string("Management VLAN unchanged\n");
				return;
			}
			port_l2_static_mgmt(uip_ethaddr.addr, management_vlan, true);
			management_vlan = vlan_settings.vlan;
			port_l2_static_mgmt(uip_ethaddr.addr, management_vlan, false);
			if (dhcp_state.state != DHCP_OFF)
				dhcp_start();
			if (!vlan_settings.vlan) {
				print_string("Management VLAN disabled\n");
			} else {
				print_string("Management VLAN set to "); print_short(management_vlan); write_char('\n');
			}
			return;
		}

		// Other commands vlan 0 is invalid
		if (!vlan_settings.vlan)
			goto err;

		if (cmd_words_len == 3 && cmd_compare(2, "d")) {
			vlan_delete(vlan_settings.vlan);
			return;
		}

		uint8_t w = 2;
		uint8_t pos = cmd_words_b[w];
		if (cmd_words_len > w && isletter(cmd_buffer[pos])) {
			uint8_t i = 0;
			while (!cmd_is_space_or_nul(pos + i))
				i++;
			if (vlan_ptr + i > (VLAN_NAMES_SIZE - 5)) {
				print_string("VLAN name table full, name ignored\n");
			} else {
				vlan_name_remove(vlan_settings.vlan);
				__xdata uint8_t *vlan_str = &vlan_names[vlan_ptr];
				*vlan_str++ = hex[(vlan_settings.vlan >> 8) & 0xf];
				*vlan_str++ = hex[(vlan_settings.vlan >> 4) & 0xf] ;
				*vlan_str++ = hex[vlan_settings.vlan & 0xf];
				while(!cmd_is_space_or_nul(pos)) {
					uint8_t c = cmd_name_char(cmd_buffer[pos++]);
					write_char(c);
					*vlan_str++ = c;
				}
				*vlan_str++ = ' '; *vlan_str = NUL;
				vlan_ptr = vlan_str - vlan_names;
				print_string("<\n");
			}
			w++;
		}

		uint8_t ret;
		uint8_t idx;
		while (cmd_words_len > w) {
			idx = cmd_words_b[w++];
			ret = cmd_arg_port(idx);
			if (ret == 0)
				goto err;

			idx += ret;
			uint16_t pmask = ((uint16_t)1) << atoi_results_u8;
			vlan_settings.members |= pmask;

			if (cmd_buffer[idx] == 't') {
				vlan_settings.tagged |= pmask;
				idx++;
			}

			if (!cmd_is_space_or_nul(idx))
				goto err;
		}
		vlan_create();
	} else if (cmd_compare(1, "show")) {
		vlan_dump();
	} else {
		goto err;
	}

	if (cmd_words_len >= 3 && isletter(cmd_buffer[cmd_words_b[2]])) {
		print_string("vlan_ptr "); print_short(vlan_ptr); write_char(':');
		write_char('>'); print_string_x(&vlan_names[0]); write_char('<'); write_char('\n');
	}
	return;
err:
	cmd_error("vlan (<vlan-id>|show) [port][t]...\n");
}

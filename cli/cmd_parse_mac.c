/*
 * "mac" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_mac.h"
#include "rtl837x_port.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata uint16_t management_vlan;

// Sets the management MAC from a "mac <aa:bb:cc:dd:ee:ff>" command (or prints
// the current one with a bare "mac"). Refuses blank/multicast/all-zero-OUI
// addresses so the running MAC stays reachable, and
// re-registers the static L2 management entry so the change applies without a
// reboot. As a regular command a "mac ..." line in the startup config applies
// on every boot (execute_config runs before the final static L2 entry).
void cmd_parse_mac(void)
{
	if (cmd_words_len == 1) {
		print_string("Current MAC: ");
		print_mac(uip_ethaddr.addr);
		write_char('\n');
		return;
	}
	if (cmd_words_len != 2 || !parse_mac(cmd_words_b[1])) {
		cmd_error("mac [<aa:bb:cc:dd:ee:ff>]\n");
		return;
	}
	if ((mac_parse_result[0] & 0x01) || ((mac_parse_result[0] | mac_parse_result[1] | mac_parse_result[2]) == 0)) {
		cmd_error("the MAC must be unicast with a non-zero first three bytes\n");
		return;
	}
	if (memcmp(mac_parse_result, uip_ethaddr.addr, 6) == 0) {
		print_string("MAC unchanged\n");
		return;
	}

	// Swap the static management entry over to the new address: remove the
	// old one (still in uip_ethaddr) before overwriting it.
	port_l2_static_mgmt(uip_ethaddr.addr, management_vlan, true);
	memcpy(uip_ethaddr.addr, mac_parse_result, 6);
	port_l2_static_mgmt(uip_ethaddr.addr, management_vlan, false);

	print_string("MAC set to: ");
	print_mac(uip_ethaddr.addr);
	write_char('\n');
	print_string("Save the startup config to keep it across reboots\n");
}

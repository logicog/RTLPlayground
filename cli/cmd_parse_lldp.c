/*
 * "lldp" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_lldp.h"
#include "rtl837x_regs.h"
#include "rtl837x_lldp.h"
#include "rtl837x_trap.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata bool lldp_enabled;

void cmd_parse_lldp(void)
{
	if (cmd_compare(1, "on")) {
		print_string("LLDP enabled\n");
		lldp_enabled = 1;
		trap_rma_set(RTL837X_RMA0E_CONF, RTL837X_RMA_ACT_TRAP);
	} else if (cmd_compare(1, "off")) {
		print_string("LLDP disabled\n");
		if (lldp_enabled)
			lldp_shutdown(lldp_logical_port_status);
		lldp_enabled = 0;
		trap_rma_set(RTL837X_RMA0E_CONF, RTL837X_RMA_ACT_FORWARD);
	} else {
		cmd_error("Usage: lldp on|off\n");
	}
}

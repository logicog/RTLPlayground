/*
 * "lacp" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_lacp.h"
#include "rtl837x_lacp.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_lacp(void)
{
	if (cmd_compare(1, "show"))
		lacp_show();
	else if (cmd_compare(1, "off"))
		lacp_off();
	else
		cmd_error("lacp (show | off); a group starts LACP with: lag <1-4> lacp <port>...\n");
}

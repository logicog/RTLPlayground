/*
 * "igmp" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_igmp.h"
#include "rtl837x_igmp.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_igmp(void)
{
	if (cmd_compare(1, "on"))
		igmp_enable();
	else if (cmd_compare(1, "off"))
		igmp_setup();
	else if (cmd_compare(1, "show"))
		igmp_show();
	else {
		cmd_error("igmp on|off|show\n");
	}
}

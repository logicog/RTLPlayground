/*
 * "l2" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_l2.h"
#include "rtl837x_port.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_l2(void)
{
	if (cmd_compare(1, "forget"))
		port_l2_forget();
	else
		port_l2_learned();
}

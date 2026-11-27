/*
 * "stat" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_stat.h"
#include "rtl837x_port.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_stat(void)
{
	port_stats_print();
}

/*
 * "pvid" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_pvid.h"
#include "rtl837x_port.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_pvid(void)
{
	if (cmd_words_len == 3 && cmd_arg_port_separator(cmd_words_b[1]) != 0
	    && atoi_short(cmd_words_b[2]) && atoi_results_short <= 4094)
		port_pvid_set(atoi_results_u8, atoi_results_short);
	else {
		cmd_error("pvid <port> <0-4094>\n");
	}
}

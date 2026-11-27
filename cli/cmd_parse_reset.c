/*
 * "reset" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_reset.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_reset(void)
{
	print_string("\nRESET\n\n");
	reset_chip();
}

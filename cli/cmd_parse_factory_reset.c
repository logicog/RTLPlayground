/*
 * "factory-reset" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_factory_reset.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_factory_reset(void)
{
	print_string("\nFACTORY RESET\n\n");
	flash_default_config();
	reset_chip();
}

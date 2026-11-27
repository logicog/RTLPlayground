/*
 * "sds" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_sds.h"
#include "rtl837x_regs.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_sds(void)
{
	print_reg(RTL837X_REG_SDS_MODES);
	write_char('\n');
}

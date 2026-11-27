/*
 * "leds" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_leds.h"
#include "rtl837x_leds.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_leds(void)
{
	leds_dump();
}

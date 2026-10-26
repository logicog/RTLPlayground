/*
 * PoE startup for boards with a PSE controller.
 *
 * The controller has no firmware of its own: the stock switch firmware keeps
 * the PSE image in its flash and downloads it over I2C after reset. The build
 * extracts that image from stock_fw/<machine>.bin (see tools/poe_extract.c),
 * so this file only has to hand the image to the chip driver selected by
 * MACHINE_POE_CHIP.
 */
#pragma codeseg BANK3
#pragma constseg BANK3

#include <stdint.h>
#include <stdbool.h>

#include "machine.h"
#include "rtl837x_common.h"

#ifndef POE_PRESENT
void poe_init(void) __banked
{
	print_string("\nNo PoE Support.\n");
}
#endif

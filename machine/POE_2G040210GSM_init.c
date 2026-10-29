/*
 * Boot hook for the MokerLink POE-2G040210GSM (2M-PCB49-V1.1).
 *
 * LED pad 6 drives port 6's LED (right SFP), and pads 0-7 are disabled at
 * reset.
 */
#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
}

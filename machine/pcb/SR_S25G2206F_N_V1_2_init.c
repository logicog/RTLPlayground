#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
	// The LED pins for port 1 (pins 3, 4), the orange LED of port 2 (pin 6) and the
	// right SFP (pin 0) are disabled in LED_GLB_IO_EN after reset, enable them as stock does
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 0);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 3);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 4);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
}

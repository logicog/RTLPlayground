#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
	// Restore the OEM LED pad routing and enables after generic LED setup.
	REG_SET(RTL837X_PIN_MUX_0, 0x20db68bf);
	REG_SET(RTL837X_REG_LED_GLB_IO_EN, 0x7f249740);
	// Select GPIO48 for the reset button instead of I2C SCL1.
	reg_bit_clear(RTL837X_PIN_MUX_1, 11);
	reg_bit_clear(RTL837X_PIN_MUX_1, 12);

	reg_bit_set(RTL837X_REG_LED_MODE, 17);
	reg_bit_clear(RTL837X_REG_LED_MODE, 9);
	reg_bit_clear(RTL837X_REG_LED_MODE, 7);
}

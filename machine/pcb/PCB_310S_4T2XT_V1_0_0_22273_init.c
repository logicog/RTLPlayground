#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
	// For this device, the reset value of RTL837X_PIN_MUX_0 is 0x30000000,
	// which would disables all LEDS, enable them manually:
	REG_SET(RTL837X_PIN_MUX_0, 0x30db68bf);
}

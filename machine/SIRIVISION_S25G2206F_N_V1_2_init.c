#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
	REG_SET(RTL837X_REG_LED_GLB_IO_EN, 0x32241559UL);
	REG_SET(RTL837X_PIN_MUX_0,         0x3ddbeaa6UL);
}
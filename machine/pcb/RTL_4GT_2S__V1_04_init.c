#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
	// The power LED on pad 28 (SYS LED) is wired active-high; the chip defaults
	// to active-low, so SYS_LED_ON would show as off. Flip pad 28's polarity.
	reg_bit_clear(RTL837X_REG_LED_GLB_ACTIVE, 28);
}

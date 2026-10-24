#include "machine_init_defs.h"

static __code const struct { uint16_t reg; uint32_t val; } custom_init_regs[] = {
	{ RTL837X_REG_LED3_0_SET1,   0x00100000UL },
	{ RTL837X_REG_LED1_0_SET1,   0x01400155UL },
	{ RTL837X_REG_LED1_0_SET0,   0x01740141UL },
	{ RTL837X_REG_LED_GLB_IO_EN, 0x7f24977fUL },
	{ RTL837X_PIN_MUX_0,         0x20db6880UL },
};

void machine_custom_init(void) __banked
{
	uint8_t i;
	// REG_SET is a multi-statement macro without a do-while wrapper: braces required
	for (i = 0; i < sizeof(custom_init_regs) / sizeof(custom_init_regs[0]); i++) {
		REG_SET(custom_init_regs[i].reg, custom_init_regs[i].val);
	}
}

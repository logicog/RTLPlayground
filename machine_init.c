/*
 * Per-machine one-shot boot hooks, hosted in BANK2 so board-specific
 * tables and code do not consume the common bank.
 */
#include <stdint.h>
#include "machine.h"
#include "rtl837x_pins.h"
#include "rtl837x_leds.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_common.h"

#pragma codeseg BANK2
#pragma constseg BANK2

#if defined(MACHINE_KP_9000_6XH_X2) || \
	defined(MACHINE_KP_9000_6XH_X2_V2_1) || \
	defined(MACHINE_KP_9000_6XHML_X2_V2_1)
void machine_custom_init(void) __banked
{
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
}

#elif defined MACHINE_PCB_SWTG018AS_V2_1_0
// Stock-firmware values for what the LED-set encoding cannot express: the
// bi-color SFP LED (blue pin at 10G) and the PIN_MUX_0 routing of that pin
// to the LED controller. Runs after leds_setup(), which covers the rest.
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

#elif defined(MACHINE_PCB_K0402WS_V3) || defined(MACHINE_HI_K0402WS)
void machine_custom_init(void) __banked
{
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
}

#elif defined(MACHINE_PCB_K0402WS_V2) || defined(MACHINE_FNS1200P)
void machine_custom_init(void) __banked
{
    reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
}

#elif defined MACHINE_PCB_SWTG024AS_A_2_0_1
void machine_custom_init(void) __banked
{
    reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
    reg_bit_set(RTL837X_REG_LED_MODE, 17);
    reg_bit_clear(RTL837X_REG_LED_MODE, 9);
    reg_bit_clear(RTL837X_REG_LED_MODE, 7);
}

#elif defined MACHINE_SWTG024AS_A_2_0_1_5C_1SFP
void machine_custom_init(void) __banked
{
    uint16_t pval;

    reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
    reg_bit_set(RTL837X_REG_LED_MODE, 17);
    reg_bit_clear(RTL837X_REG_LED_MODE, 9);
    reg_bit_clear(RTL837X_REG_LED_MODE, 7);

    // OEM firmware sets these companion SDS0 polarity bits for the RTL8221B.
    sds_read(0, 0, 0);
    pval = SFR_DATA_U16;
    sds_write_v(0, 0, 0, pval | 0x100);

    sds_read(0, 6, 2);
    pval = SFR_DATA_U16;
    sds_write_v(0, 6, 2, pval | 0x4000);
}

#elif defined MACHINE_SWTG024AS_V2_0
void machine_custom_init(void) __banked
{
    uint16_t pval;

    reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
    reg_bit_set(RTL837X_REG_LED_MODE, 17);
    reg_bit_clear(RTL837X_REG_LED_MODE, 9);
    reg_bit_clear(RTL837X_REG_LED_MODE, 7);

    // OEM firmware sets these companion SDS0 polarity bits for the RTL8221B.
    sds_read(0, 0, 0);
    pval = SFR_DATA_U16;
    sds_write_v(0, 0, 0, pval | 0x100);

    sds_read(0, 6, 2);
    pval = SFR_DATA_U16;
    sds_write_v(0, 6, 2, pval | 0x4000);
}

#elif defined MACHINE_ZX310S_4T2XT
void machine_custom_init(void) __banked
{
	// For this device, the reset value of RTL837X_PIN_MUX_0 is 0x30000000,
	// which would disables all LEDS, enable them manually:
	REG_SET(RTL837X_PIN_MUX_0, 0x30db68bf);
}

#elif defined MACHINE_FG_4GT_2SX_V2_0
void machine_custom_init(void) __banked
{
	REG_SET(RTL837X_REG_LED_GLB_IO_EN, 0x7624155b);
}

#else
void machine_custom_init(void) __banked { }
#endif

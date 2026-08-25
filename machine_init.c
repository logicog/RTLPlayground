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

#if defined MACHINE_KP_9000_6XH_X2
void machine_custom_init(void) __banked
{
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
}

#elif defined MACHINE_PCB_SWTG018AS_V2_1_0
// The LED-set encoding cannot express this board's bi-color SFP LED (green <= 2.5G,
// blue at 10G), so program the LED register block with the values the stock firmware
// uses. Runs after leds_setup() and overrides the values computed there.
// The final entry routes the blue-LED pin to the LED controller via PIN_MUX_0;
// as a GPIO (the default) no LED register can drive it. PIN_MUX_1/2 stay
// untouched so SFP detect (GPIO38) and i2c remain GPIOs.
static __code const struct { uint16_t reg; uint32_t val; } custom_init_regs[] = {
	{ 0x6520, 0x0023e430UL },   // LED_MODE
	{ 0x6524, 0xff001400UL },   // LED3_0_SET3
	{ 0x6528, 0x00100000UL },   // LED3_0_SET1
	{ 0x652c, 0x007f013fUL },   // LED3_2_SET3
	{ 0x6530, 0x02000400UL },   // LED1_0_SET3
	{ 0x6534, 0x01400141UL },   // LED3_2_SET2
	{ 0x6538, 0x01440170UL },   // LED1_0_SET2
	{ 0x653c, 0x18000041UL },   // LED3_2_SET1
	{ 0x6540, 0x01400155UL },   // LED1_0_SET1
	{ 0x6544, 0x01411000UL },   // LED3_2_SET0
	{ 0x6548, 0x01740141UL },   // LED1_0_SET0
	{ 0x654c, 0x00010000UL },   // LED_PORT_SET_SEL
	{ 0x65d8, 0x3ffb6dffUL },   // LED_GLB_ACTIVE
	{ 0x65dc, 0x7f24977fUL },   // LED_GLB_IO_EN
	{ 0x65e0, 0x08144040UL },   // LED_GLB_MUX_1
	{ 0x65e4, 0x10349309UL },   // LED_GLB_MUX_2
	{ 0x65e8, 0x12454391UL },   // LED_GLB_MUX_3
	{ 0x65ec, 0x19616555UL },   // LED_GLB_MUX_4
	{ 0x65f0, 0x1c79d65aUL },   // LED_GLB_MUX_5
	{ 0x65f4, 0x0002181dUL },   // LED_GLB_MUX_6
	{ 0x7f8c, 0x20db6880UL },   // PIN_MUX_0
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

#elif defined MACHINE_FNS1200P
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

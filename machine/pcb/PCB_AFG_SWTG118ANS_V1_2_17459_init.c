#include "machine_init_defs.h"

/*
 * PCB-AFG-SWTG118ANS-V1.2-17459.
 * Runs after leds_setup(). The OEM V100.9.9 LED init sets LED_MODE (0x6520)
 * bits [12:9] = 0 and [8:5] = 7, where RTLPlayground's default has 3 and 5.
 * LED_GLB_IO_EN is left at its default here (the V2.0.2 board overwrites it).
 */
void machine_custom_init(void) __banked
{
	reg_read_m(RTL837X_REG_LED_MODE);
	sfr_mask_data(0, 0xe0, 0xe0);	// bits 7:5 = 1 (with bit 8 = 0: field [8:5] = 7)
	sfr_mask_data(1, 0x1f, 0x00);	// bit 8 = 0, bits 12:9 = 0
	reg_write_m(RTL837X_REG_LED_MODE);
}

#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
	// Stock LED_MODE: [8:5]=1, [12:9]=2 (leds_setup hard-codes 5/3); same as clearing bits 7 and 9
	reg_bit_set(RTL837X_REG_LED_MODE, 17);
	reg_bit_clear(RTL837X_REG_LED_MODE, 9);
	reg_bit_clear(RTL837X_REG_LED_MODE, 7);

	// Stock LED_GLB_IO_EN [27:0] = 0x0f249740: enable LED pins 6, 8, 9, 10, 12, 15, 18, 21, 24-27
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 8);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 9);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 10);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 12);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 15);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 18);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 21);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 24);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 25);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 26);
}
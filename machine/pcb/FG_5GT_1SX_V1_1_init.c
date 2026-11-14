#include "machine_init_defs.h"

void machine_custom_init(void) __banked
{
	/* Enable the 12 LED pads of the six ports:
	 * 0,1 3,4 6,8 10,12 18,21 25,26 -> LED_GLB_IO_EN 0x65dc = 0x3624155b.
	 * Only set bits, a full write clears the reset defaults and takes down
	 * port 5 (RTL8221B) together with the management access through it.
	 */
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 0);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 1);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 3);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 4);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 6);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 8);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 10);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 12);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 18);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 21);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 25);
	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 26);

	// The stock LED_MODE keeps bits 7 and 9 set, the OEM profile clears them
	reg_bit_set(RTL837X_REG_LED_MODE, 17);
	reg_bit_clear(RTL837X_REG_LED_MODE, 9);
	reg_bit_clear(RTL837X_REG_LED_MODE, 7);
}

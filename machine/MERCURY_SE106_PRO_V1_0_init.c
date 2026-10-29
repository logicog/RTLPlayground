#include "machine_init_defs.h"

// Match the stock LED setup: pads 26/27 have the inverted active level,
// pads 0-27 and bit 30 are IO enabled. Bits 28/29 stay as leds_setup() left them.
void machine_custom_init(void) __banked
{
	reg_read_m(RTL837X_REG_LED_GLB_ACTIVE);
	sfr_mask_data(3, 0x0f, 0x0c);
	sfr_mask_data(2, 0xff, 0x00);
	sfr_mask_data(1, 0xff, 0x00);
	sfr_mask_data(0, 0xff, 0x00);
	reg_write_m(RTL837X_REG_LED_GLB_ACTIVE);

	reg_read_m(RTL837X_REG_LED_GLB_IO_EN);
	sfr_mask_data(3, 0x4f, 0x4f);
	sfr_mask_data(2, 0xff, 0xff);
	sfr_mask_data(1, 0xff, 0xff);
	sfr_mask_data(0, 0xff, 0xff);
	reg_write_m(RTL837X_REG_LED_GLB_IO_EN);

	// Stock TX polarity of the copper PHYs, the inverse of the reset value 0xa695
	REG_SET(RTL837X_CFG_PHY_TX_POLARITY_SWAP, 0x0000596a);
}

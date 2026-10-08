#include "machine_defs.h"

__code const struct machine machine = {
	.machine_name = MACHINE_MODEL,
	.isRTL8373 = 0,
	.min_port = 3,
	.max_port = 8,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp = {0, 0, 0, 1, 0, 0, 0, 0, 2},
	.sfp_port[0].pin_detect = GPIO38,
	.sfp_port[0].pin_los = GPIO50_I2C_SCL2_UART1_TX,
	.sfp_port[0].pin_tx_disable = GPIO54_ACL_BIT2_EN,
	.sfp_port[0].sds = 0,
	.sfp_port[0].i2c = I2CBUS( GPIO47_I2C_SDA0, GPIO46_I2C_SCL0 ),
	.sfp_port[1].pin_detect = GPIO36_PWM_OUT,
	.sfp_port[1].pin_los = GPIO37,
	.sfp_port[1].pin_tx_disable = GPIO51_I2C_SDA2_UART1_RX,
	.sfp_port[1].sds = 1,
	.sfp_port[1].i2c = I2CBUS( GPIO49_I2C_SDA1, GPIO48_I2C_SCL1 ),
	.reset_pin = GPIO_NA,
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1},
	.led_sets = {
		{
			0, // Unused
			LEDS_2G5 | LEDS_LINK | LEDS_ACT, // Green
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT, // Amber
			0 // Unused
		},
		{
			0, // Unused
			0, // Unused
			LEDS_10G | LEDS_1G | LEDS_LINK | LEDS_ACT, // Green
			0, // Unused
		},
	},

};

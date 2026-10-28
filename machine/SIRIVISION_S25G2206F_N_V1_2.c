#include "machine_defs.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp= {0, 0, 0, 2, 0, 0, 0, 0, 1},

	// Left SFP port, SerDes 1 / port 8
  .sfp_port[0].pin_detect = GPIO38,
  .sfp_port[0].pin_los = GPIO_NA,
  .sfp_port[0].pin_tx_disable = GPIO_NA,
  .sfp_port[0].sds = 1,
  .sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	// Right SFP port, SerDes 0 / port 3
  .sfp_port[1].pin_detect = GPIO37,
  .sfp_port[1].pin_los = GPIO_NA,
  .sfp_port[1].pin_tx_disable = GPIO_NA,
  .sfp_port[1].sds = 0,
  .sfp_port[1].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),
  // Reset may be on GPIO36_PWM_OUT based on similar boards - but not verified
	.reset_pin = GPIO_NA,
	.high_leds = { .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1},
  .led_sets = {
    { LEDS_ACT | LEDS_LINK | LEDS_10M | LEDS_100M | LEDS_500M | LEDS_1G,
      LEDS_2G5,
      0,
      0 },
    { LEDS_ACT | LEDS_LINK | LEDS_10M | LEDS_100M | LEDS_500M | LEDS_1G | LEDS_2G5 | LEDS_5G,
      LEDS_10G,
      0,
      0 },
  },
  .led_mux_custom = 1,
  .led_mux = {
    0x0c, 0x01, 0x04, 0x10, 0x11, 0x14, 0x14, 0x3f, 0x15, 0x10,
    0x18, 0x3f, 0x19, 0x3f, 0x3f, 0x15, 0x3f, 0x3f, 0x1c, 0x3f,
    0x3f, 0x1d, 0x3f, 0x3f, 0x1c, 0x20, 0x20, 0x21
  },
};
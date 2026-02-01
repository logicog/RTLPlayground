#include "machine.h"
#include "rtl837x_pins.h"
#include "rtl837x_leds.h"

#ifdef MACHINE_KP_9000_6XHML_X2
__code const struct machine machine = {
	.machine_name = "keepLink KP-9000-6XHML-X2",
	.isRTL8373 = 0,
	.min_port = 3,
	.max_port = 8,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 5, 1, 2, 3, 4, 6},
	.phys_to_log_port = {4, 5, 6, 7, 3, 8, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO50_I2C_SCL2_UART1_TX,
	.sfp_port[0].pin_los = GPIO10_LED10,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 0,
	.sfp_port[0].i2c = { .sda = GPIO41_I2C_SDA3_MDIO1, .scl = GPIO40_I2C_SCL3_MDC1 },
	.sfp_port[1].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[1].pin_los = GPIO37,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 1,
	.sfp_port[1].i2c = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 },
	.reset_pin = GPIO46_I2C_SCL0,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28 | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 0},
	/* Conditions for LED on: 
	 * dual led orange: ledset_0 & ledset_2
	 * dual led green: ledset_2 & !ledset_0
	 * single right led green: ledset_0 & !ledset_1
	*/
	.led_sets = { { LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			LEDS_2G5 | LEDS_LINK | LEDS_10G,
			LEDS_1G | LEDS_LINK, 
			0 },
		    },
};
#elif defined MACHINE_KP_9000_6XH_X
__code const struct machine machine = {
	.machine_name = "keepLink KP-9000-6XH-X",
	.isRTL8373 = 0,
	.min_port = 3,
	.max_port = 8,
	.n_sfp = 1,
	.log_to_phys_port = {0, 0, 0, 5, 1, 2, 3, 4, 6},
	.phys_to_log_port = {4, 5, 6, 7, 3, 8, 0, 0, 0},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c_bus = { .sda = 4, .scl = 3 },
	.sfp_port[0].i2c = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 },
	.reset_pin = GPIO_NA,
	/* Conditions for LED on: 
	 * dual led orange: ledset_0 & ledset_2
	 * dual led green: ledset_2 & !ledset_0
	 * single right led green: ledset_0 & !ledset_1
	*/
	.led_sets = { { LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			LEDS_2G5 | LEDS_LINK | LEDS_10G,
			LEDS_1G | LEDS_LINK, 
			0 },
		    },
};
#elif defined MACHINE_KP_9000_9XH_X_EU
__code const struct machine machine = {
	.machine_name = "keepLink KP-9000-6XH-X-EU",
	.isRTL8373 = 1,
	.min_port = 0,
	.max_port = 8,
	.n_sfp = 1,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 },
	.reset_pin = GPIO_NA,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28 | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.led_sets = { { LEDS_2G5 | LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5,
			LEDS_2G5 | LEDS_LINK,
			LEDS_1G | LEDS_LINK, 
			LEDS_2G5 | LEDS_LINK | LEDS_ACT },
		    },
};

#elif defined MACHINE_SWGT024_V2_0
__code const struct machine machine = {
	.machine_name = "SWGT024 V2.0",
	.isRTL8373 = 0,
	.min_port = 3,
	.max_port = 8,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp= {0, 0, 0, 2, 0, 0, 0, 0, 1},
	// Left SFP port (J4)
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 }, /* GPIO 39 */
	// Right SFP port (J2)
	.sfp_port[1].pin_detect = GPIO50_I2C_SCL2_UART1_TX,
	.sfp_port[1].pin_los = GPIO51_I2C_SDA2_UART1_RX,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = { .sda = GPIO41_I2C_SDA3_MDIO1, .scl = GPIO40_I2C_SCL3_MDC1 }, /* GPIO 40 */
	.reset_pin = GPIO36_PWM_OUT,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28 | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 0},
	/* Conditions for LED on: 
	 * dual led orange: ledset_0 & ledset_2
	 * dual led green: ledset_2 & !ledset_0
	 * single right led green: ledset_0 & !ledset_1
	*/
	.led_sets = { { LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			LEDS_2G5 | LEDS_LINK | LEDS_10G,
			LEDS_1G | LEDS_LINK, 
			0 },
		    },
};

#elif defined MACHINE_HG0402XG_V1_1
__code const struct machine machine = {
	.machine_name = "HG0402XG V1.1",
	.isRTL8373 = 0,
	.min_port = 3,
	.max_port = 8,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 5, 1, 2, 3, 4, 6},
	.phys_to_log_port = {4, 5, 6, 7, 3, 8, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = 50,
	.sfp_port[0].pin_los = 10,
	.sfp_port[0].pin_tx_disable = 0xFF,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c_bus ={ .sda = GPIO41_I2C_SDA3_MDIO1, .scl = GPIO40_I2C_SCL3_MDC1 },
	.sfp_port[1].pin_detect = 30,
	.sfp_port[1].pin_los = 51,
	.sfp_port[1].pin_tx_disable = 0xFF,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c_bus = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 },
	.reset_pin = GPIO_NA,
	.high_leds = { .mux = LED_27 , .enable = LED_27 | LED_29 },
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1},
	/* The Ethernet ports have 1 amber LED (left) and 1 green LED (right)
	 * The SFP ports have also 1 amber LED and 1 green LED
	 * Ethernet ports use LED-set 0, SFP ports use LED-set 1
	 */
	.led_sets = { { LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_2G5 | LEDS_LINK | LEDS_ACT,
			LEDS_2G5 | LEDS_LINK | LEDS_ACT, 
			0 },
			{ LEDS_100M | LEDS_10M | LEDS_LINK,
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			LEDS_10G | LEDS_LINK, 
			0 },
		    },
};

#elif defined DEFAULT_8C_1SFP
__code const struct machine machine = {
	.machine_name = "8+1 SFP Port Switch",
	.isRTL8373 = 1,
	.min_port = 0,
	.max_port = 8,
	.n_sfp = 1,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 },
	.reset_pin = GPIO_NA,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28 | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.led_sets = { { LEDS_2G5 | LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5,
			LEDS_2G5 | LEDS_LINK,
			LEDS_1G | LEDS_LINK, 
			LEDS_2G5 | LEDS_LINK | LEDS_ACT },
		    },
};
#endif

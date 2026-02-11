#include "machine.h"
#include "rtl837x_pins.h"
#include "rtl837x_leds.h"
#include "rtl837x_regs.h"

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
	.led_mux_custom = 1,
	.led_mux = {0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x0f, 0x0c, 0x0d, 0x0e, 0x10, 0x11, 0x12, 0x14,
		    0x15, 0x16, 0x18, 0x19, 0x1a, 0x1c, 0x1d, 0x1e, 0x20, 0x21, 0x22, 0x23 },
};

void machine_custom_init(void) { }

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

void machine_custom_init(void) { }

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
#elif defined MACHINE_KP_9000_9XHML_X
__code const struct machine machine = {
	.machine_name = "keepLink KP-9000-9XHML-X",
	.isRTL8373 = 1,
	.min_port = 0,
	.max_port = 8,
	.n_sfp = 1,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO38,
	.sfp_port[0].pin_los = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 },
	.reset_pin = GPIO48_I2C_SCL1,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28 | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 1},
	.led_sets = {
		{   /* RJ45: First LED, yellow, second LED: green */
			LEDS_2G5 | LEDS_LINK,
            LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			0,
			0,
		}, { /* SFP PORT, SINGLE GREEN LED */
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			0,
			0,
			0,
		}},
	.led_mux_custom = 1,
	.led_mux = {0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x0c, 0x09, 0x0d, 0x10,
				0x11, 0x0e, 0x14, 0x11, 0x12, 0x15, 0x15, 0x16, 0x18, 0x19,
				0x1a, 0x19, 0x1d, 0x1e, 0x1c, 0x1d, 0x20, 0x21},
};

void machine_custom_init(void) { }

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

void machine_custom_init(void) { }

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

void machine_custom_init(void) { }

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

void machine_custom_init(void) { }

#elif defined MACHINE_TRENDNET_TEG_S562
__code const struct machine machine = {
	.machine_name = "Trendnet TEG-S562",
	.isRTL8373 = 0,
	.min_port = 3,
	.max_port = 8,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 3, 8, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO36_PWM_OUT,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO51_I2C_SDA2_UART1_RX,
	.sfp_port[0].sds = 0,
	.sfp_port[0].i2c = { .sda = GPIO47_I2C_SDA0, .scl = GPIO46_I2C_SCL0 },
	.sfp_port[1].pin_detect = GPIO38,
	.sfp_port[1].pin_los = GPIO50_I2C_SCL2_UART1_TX,
	.sfp_port[1].pin_tx_disable = GPIO54_ACL_BIT2_EN,
	.sfp_port[1].sds = 1,
	.sfp_port[1].i2c = { .sda = GPIO49_I2C_SDA1, .scl = GPIO48_I2C_SCL1 },
	.reset_pin = GPIO_NA,
};

void machine_custom_init(void) { }

#endif

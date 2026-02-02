#include "machine.h"
#include "rtl837x_pins.h"

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
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = { .sda = GPIO41_I2C_SDA3_MDIO1, .scl = GPIO40_I2C_SCL3_MDC1 },
	.sfp_port[1].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[1].pin_los = GPIO37,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 },
	.reset_pin = GPIO46_I2C_SCL0,
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
	.sfp_port[0].i2c = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 },
	.reset_pin = GPIO_NA,
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
};
#elif defined MACHINE_HI_K0402WS
__code const struct machine machine = {
	.machine_name = "HiSource HI-K0402WS",
	.isRTL8373 = 0,
	.min_port = 3,
	.max_port = 8,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},
	
	// Left SFP port
	.sfp_port[0].pin_detect = 38, // Checked !
	.sfp_port[0].pin_los = 27, // checked !
	.sfp_port[0].sds = 1, 
	.sfp_port[0].i2c = 0, //checked

	// Right SFP port
	.sfp_port[1].pin_detect = 37, // Checked !
	.sfp_port[1].pin_los = 8, // checked !
	.sfp_port[1].sds = 0, 
	.sfp_port[1].i2c = 1, // checked correct

	.reset_pin = 46,

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
};
#endif

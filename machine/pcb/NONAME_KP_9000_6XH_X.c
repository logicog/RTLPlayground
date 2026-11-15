#include "machine_defs.h"

#define RTL8221_PTP
#include "rtl8221_ptp.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 1,
	.log_to_phys_port = {0, 0, 0, 5, 1, 2, 3, 4, 6},
	.phys_to_log_port = {4, 5, 6, 7, 3, 8, 0, 0, 0},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
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
	.n_ptp = 2,
	.ptp[0].init = ptp_init,
	.ptp[0].read = ptp_read_time,
	.ptp[0].get = ptp_get_time,
	.ptp[0].set = ptp_set_time,
	.ptp[1].phy = 3,
	.ptp[1].init = ptp_rtl8221_init,
	.ptp[1].read = ptp_rtl8221_read_time,
	.ptp[1].get = ptp_rtl8221_get_time,
	.ptp[1].set = ptp_rtl8221_set_time,
};

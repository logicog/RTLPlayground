/*
 * This is a driver implementation for the IGMP features for the RTL827x platform
 * This code is in the Public Domain
 */

// #define REGDBG
// #define DEBUG

#define IPMC_USES_L3MC

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_leds.h"
#include "machine.h"

extern __code const struct machine machine;

#include "uip.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata uint8_t sfr_data[4];


void leds_setup(void) __banked
{
	print_string("leds_setup called\n");
	REG_SET(RTL837X_REG_LED_MODE, 0x0021e6b0);

	// Disable RLDP (Realtek Loop Detection Protocol) LEDs on loop detection
	reg_read_m(RTL837X_REG_LED_RLDP_1);
	sfr_mask_data(0, 0x03, 0);
	reg_write_m(RTL837X_REG_LED_RLDP_1);

	// Set up all Port-LEDs to belong to RLDP
	sfr_data[3] = sfr_data[2] = sfr_data[1] = sfr_data[0] = 0;
	for (uint8_t i = machine.min_port; i <= (machine.max_port > 7 ? 7 : machine.max_port); i++)
		sfr_data[3 - (i >> 2)] |= i & 1 ? 0xf0 : 0x0f;
	reg_write_m(RTL837X_REG_LED_RLDP_2);
	if (machine.max_port == 8)
		REG_SET(RTL837X_REG_LED_RLDP_3, 0x0000000f);	// Port 8

	// Configure high LEDs 27-29: mux and LED enable
	if (machine.high_leds.mux & LED_27)
		reg_bit_set(RTL837X_PIN_MUX_0, 27);
	else
		reg_bit_clear(RTL837X_PIN_MUX_0, 27);

	// SYSTEM LED
	if (machine.high_leds.mux & LED_28_SYS)
		reg_bit_set(RTL837X_PIN_MUX_0, 28);
	else
		reg_bit_clear(RTL837X_PIN_MUX_0, 28);

	if (machine.high_leds.mux & LED_29)
		reg_bit_set(RTL837X_PIN_MUX_0, 29);
	else
		reg_bit_clear(RTL837X_PIN_MUX_0, 29);

	if (machine.high_leds.enable & LED_27)
		reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 27);
	else
		reg_bit_clear(RTL837X_REG_LED_GLB_IO_EN, 27);

	// SYSTEM LED
	if (machine.high_leds.enable & LED_28_SYS)
		reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 28);
	else
		reg_bit_clear(RTL837X_REG_LED_GLB_IO_EN, 28);

	if (machine.high_leds.enable & LED_29)
		reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 29);
	else
		reg_bit_clear(RTL837X_REG_LED_GLB_IO_EN, 29);

	// Configure the LED-mux
	if (machine.led_mux_custom) {
		print_string("Configuring custom LED-muxes: ");
		for (uint8_t i = 0; i < 28; i++) {
			switch (i % 5) {
			case 0:  // 0
				sfr_data[3] = machine.led_mux[i];
				break;
			case 1:  // 6
				sfr_data[3] |= machine.led_mux[i] << 6;
				sfr_data[2] = machine.led_mux[i] >> 2;
				break;
			case 2:  // 12
				sfr_data[2] |= machine.led_mux[i] << 4;
				sfr_data[1] = machine.led_mux[i] >> 4;
				break;
			case 3:  // 18
				sfr_data[1] |= machine.led_mux[i] << 2;
				break;
			case 4:  // 24
				sfr_data[0] = machine.led_mux[i];
				print_sfr_data(); write_char(' ');
				reg_write_m(RTL837X_REG_LED_GLB_MUX_1 + (i / 5) * 4);
				break;
			}
			write_char(' ');
		}
		sfr_data[0] = 0; sfr_data[1] &= 0xf;
		print_sfr_data(); write_char('\n');
		reg_write_m(RTL837X_REG_LED_GLB_MUX_6);
	}
	
	// Configure the LED-set of a port
	sfr_data[3] = sfr_data[2] = sfr_data[1] = sfr_data[0] = 0;
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++)
		sfr_data[3 - (i >> 2)] |= machine.port_led_set[i] << ((i & 3) << 1);
	reg_write_m(RTL837X_LED_PORT_SET_SEL);

	// Configure the LED-sets
	sfr_data[3] = sfr_data[2] = sfr_data[1] = sfr_data[0] = 0;
	reg_write_m(RTL837X_REG_LED3_0_SET1);
	reg_write_m(RTL837X_REG_LED3_0_SET3);
	__code const uint8_t * __xdata lptr = &machine.led_sets[0][0];
	for (__xdata uint8_t set = 0; set < 4; set++) {
		sfr_data[0] = *(lptr + 5);
		sfr_data[1] = *(lptr + 4);
		sfr_data[2] = *(lptr + 1);
		sfr_data[3] = *(lptr);
		reg_write_m(RTL837X_REG_LED1_0_SET0 - set * 8);

		if (set < 2) {
			reg_read_m(RTL837X_REG_LED3_0_SET1);
			sfr_data[3 - (set << 1)] = (*(lptr + 6) << 4) | (*(lptr + 2));
			reg_write_m(RTL837X_REG_LED3_0_SET1);
		} else {
			reg_read_m(RTL837X_REG_LED3_0_SET3);
			sfr_data[3 - ((set - 2) << 1)] = (*(lptr + 6) << 4) | (*(lptr + 2));
			reg_write_m(RTL837X_REG_LED3_0_SET3);
		}
		lptr += 8;
		sfr_data[0] = *(lptr + 5);
		sfr_data[1] = *(lptr + 4);
		sfr_data[2] = *(lptr + 1);
		sfr_data[3] = *(lptr);
		reg_write_m(RTL837X_REG_LED1_0_SET0 - set * 8 - 4);

		if (set < 2) {
			reg_read_m(RTL837X_REG_LED3_0_SET1);
			sfr_data[2 - (set << 1)] = (*(lptr + 6) << 4) | (*(lptr + 2));
			reg_write_m(RTL837X_REG_LED3_0_SET1);
		} else {
			reg_read_m(RTL837X_REG_LED3_0_SET3);
			sfr_data[2 - ((set - 2) << 1)] = (*(lptr + 6) << 4) | (*(lptr + 2));
			reg_write_m(RTL837X_REG_LED3_0_SET3);
		}
		lptr += 8;
	}
	print_string("leds_setup done\n");
}

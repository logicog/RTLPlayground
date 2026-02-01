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

extern __code struct machine machine;

#include "uip.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata uint8_t sfr_data[4];

void leds_dump(void) __banked
{
	print_string("RTL837X_PIN_MUX_0: "); print_reg(RTL837X_PIN_MUX_0); write_char('\n');
	print_string("RTL837X_REG_LED_GLB_IO_EN: "); print_reg(RTL837X_REG_LED_GLB_IO_EN); write_char('\n');
	print_string("RTL837X_REG_LED1_0_SET0: "); print_reg(RTL837X_REG_LED1_0_SET0); write_char('\n');
	print_string("RTL837X_REG_LED3_2_SET0: "); print_reg(RTL837X_REG_LED3_2_SET0); write_char('\n');
	print_string("RTL837X_REG_LED1_0_SET1: "); print_reg(RTL837X_REG_LED1_0_SET1); write_char('\n');
	print_string("RTL837X_REG_LED3_2_SET1: "); print_reg(RTL837X_REG_LED3_2_SET1); write_char('\n');
	print_string("RTL837X_REG_LED1_0_SET2: "); print_reg(RTL837X_REG_LED1_0_SET2); write_char('\n');
	print_string("RTL837X_REG_LED3_2_SET2: "); print_reg(RTL837X_REG_LED3_2_SET2); write_char('\n');
	print_string("RTL837X_REG_LED1_0_SET3: "); print_reg(RTL837X_REG_LED1_0_SET3); write_char('\n');
	print_string("RTL837X_REG_LED3_0_SET1: "); print_reg(RTL837X_REG_LED3_0_SET1); write_char('\n');
	print_string("RTL837X_REG_LED3_0_SET3: "); print_reg(RTL837X_REG_LED3_0_SET3); write_char('\n');
	print_string("RTL837X_LED_PORT_SET_SEL: "); print_reg(RTL837X_LED_PORT_SET_SEL); write_char('\n');
	print_string("LED Configuration:\n");
	print_string("LED-ID\t\t0\t\t1\t\t2\t\t3\n");
	for (__xdata uint8_t set = 0; set < 4; set++) {
		print_string("SET "); write_char('0' + set); write_char(':');
		for (__xdata uint8_t ledid = 0; ledid < 4; ledid++) {
			print_string("\t   ");
			uint8_t b;
			if (set < 2) {
				reg_read_m(RTL837X_REG_LED3_0_SET1);
				b = sfr_data[3-((set << 1) + (ledid >> 1))];
				print_byte(ledid & 1 ? b >> 4 : b & 0xf);
			} else {
				reg_read_m(RTL837X_REG_LED3_0_SET3);
				b = sfr_data[3-(((set-2) << 1) + (ledid >> 1))];
				print_byte(ledid & 1 ? b >> 4 : b & 0xf);
			}
			reg_read_m(RTL837X_REG_LED1_0_SET0 - set * 8 - ((ledid >> 1) * 4));
			if (! (ledid & 1)) { // LEDID 0, 2
				print_byte(sfr_data[2]); print_byte(sfr_data[3]);
			} else {
				print_byte(sfr_data[0]); print_byte(sfr_data[1]);
			}
		}
		write_char('\n');
	}
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		reg_read_m(RTL837X_LED_PORT_SET_SEL);
		__xdata uint8_t set = sfr_data[3 - (i >> 2)];
		set = (set >> ((i & 3) << 1));
		print_string("Port "); write_char('0' + i); print_string(": SET ");
		write_char('0' + set);
		print_string(": ");
		for (__xdata uint8_t ledid = 0; ledid < 4; ledid++) {
			write_char('(');
			reg_read_m(RTL837X_REG_LED1_0_SET0 - set * 8 - ((ledid >> 1) * 4));
			if (ledid & 1) {  // LEDID 1, 3
				sfr_data[2] = sfr_data[0];
				sfr_data[3] = sfr_data[1];
			}
			if (sfr_data[3] & 0x01)
				print_string(" 2G5");
			if (sfr_data[3] & 0x02)
				print_string(" TWO_1G");
			if (sfr_data[3] & 0x04)
				print_string(" 1G");
			if (sfr_data[3] & 0x08)
				print_string(" 500M");
			if (sfr_data[3] & 0x10)
				print_string(" 100M");
			if (sfr_data[3] & 0x20)
				print_string(" 10M");
			if (sfr_data[3] & 0x40)
				print_string(" LINK");
			if (sfr_data[3] & 0x80)
				print_string(" LINK_FLASH");
			if (sfr_data[2] & 0x01)
				print_string(" ACT");
			if (sfr_data[2] & 0x02)
				print_string(" RX");
			if (sfr_data[2] & 0x04)
				print_string(" TX");
			if (sfr_data[2] & 0x08)
				print_string(" COL");
			if (sfr_data[2] & 0x10)
				print_string(" DUPLEX");
			if (sfr_data[2] & 0x20)
				print_string(" TRAINING");
			if (sfr_data[2] & 0x40)
				print_string(" MASTER");
			__xdata uint8_t b;
			if (set < 2) {
				reg_read_m(RTL837X_REG_LED3_0_SET1);
				b = sfr_data[3-((set << 1) + (ledid >> 1))];
			} else {
				reg_read_m(RTL837X_REG_LED3_0_SET3);
				b = sfr_data[3-(((set-2) << 1) + (ledid >> 1))];
			}
			b = ledid & 1 ? b >> 4 : b & 0xf;
			if (b & 0x1)
				print_string(" 10G");
			if (b & 0x2)
				print_string(" TWO_5G");
			if (b & 0x4)
				print_string(" 5G");
			if (b & 0x8)
				print_string(" TWO_2G5");
			print_string("), ");
		}
		write_char('\n');
	}
}


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

	if (machine.high_leds.mux & LED_28)
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

	if (machine.high_leds.enable & LED_28)
		reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 28);
	else
		reg_bit_clear(RTL837X_REG_LED_GLB_IO_EN, 28);

	if (machine.high_leds.enable & LED_29)
		reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 29);
	else
		reg_bit_clear(RTL837X_REG_LED_GLB_IO_EN, 29);
		

	// Configure the LED-set of a port
	sfr_data[3] = sfr_data[2] = sfr_data[1] = sfr_data[0] = 0;
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++)
		sfr_data[3 - (i >> 2)] |= machine.port_led_set[i] << ((i & 3) << 1);
	reg_write_m(RTL837X_LED_PORT_SET_SEL);

	// Configure the LED-sets
	__code uint8_t * __xdata lptr = &machine.led_sets[0][0];
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
			sfr_data[3 - (set << 1)] = (*(lptr + 6) << 4) | (*(lptr + 2));
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
			sfr_data[2 - (set << 1)] = (*(lptr + 6) << 4) | (*(lptr + 2));
			reg_write_m(RTL837X_REG_LED3_0_SET3);
		}
		lptr += 8;
	}
	print_string("leds_setup done\n");
}

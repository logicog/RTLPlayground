#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_leds.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code const struct machine machine;
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
	print_string("RTL837X_REG_LED3_2_SET3: "); print_reg(RTL837X_REG_LED3_2_SET3); write_char('\n');
	print_string("RTL837X_REG_LED3_0_SET1: "); print_reg(RTL837X_REG_LED3_0_SET1); write_char('\n');
	print_string("RTL837X_REG_LED3_0_SET3: "); print_reg(RTL837X_REG_LED3_0_SET3); write_char('\n');
	print_string("RTL837X_LED_PORT_SET_SEL: "); print_reg(RTL837X_LED_PORT_SET_SEL); write_char('\n');
	print_string("RTL837X_REG_LED_GLB_MUX_1: "); print_reg(RTL837X_REG_LED_GLB_MUX_1); write_char('\n');
	print_string("RTL837X_REG_LED_GLB_MUX_2: "); print_reg(RTL837X_REG_LED_GLB_MUX_2); write_char('\n');
	print_string("RTL837X_REG_LED_GLB_MUX_3: "); print_reg(RTL837X_REG_LED_GLB_MUX_3); write_char('\n');
	print_string("RTL837X_REG_LED_GLB_MUX_4: "); print_reg(RTL837X_REG_LED_GLB_MUX_4); write_char('\n');
	print_string("RTL837X_REG_LED_GLB_MUX_5: "); print_reg(RTL837X_REG_LED_GLB_MUX_5); write_char('\n');
	print_string("RTL837X_REG_LED_GLB_MUX_6: "); print_reg(RTL837X_REG_LED_GLB_MUX_6); write_char('\n');
	print_string("RTL837X_REG_LED_GLB_ACTIVE: "); print_reg(RTL837X_REG_LED_GLB_ACTIVE); write_char('\n');
	print_string("RTL837X_REG_LED_MODE: "); print_reg(RTL837X_REG_LED_MODE); write_char('\n');
	print_string("LED pad Configuration:\n");
	for (uint8_t i = 0; i < 28; i++) {
		print_byte(i);
		write_char(' ');
	}
	write_char('\n');
	for (uint8_t i = 0; i < 28; i++) {
		switch (i % 5) {
		case 0:  // 0
			reg_read_m(RTL837X_REG_LED_GLB_MUX_1 + (i / 5) * 4);
			print_byte(sfr_data[3] & 0x3f);
			break;
		case 1:  // 6
			print_byte(((sfr_data[3] >> 6) | (sfr_data[2] << 2)) & 0x3f);
			break;
		case 2:  // 12
			print_byte(((sfr_data[1] << 4) | (sfr_data[2] >> 4)) & 0x3f);
			break;
		case 3:  // 18
			print_byte((sfr_data[1] >> 2) & 0x3f);
			break;
		case 4:  // 24
			print_byte(sfr_data[0] & 0x3f);
			break;
		}
		write_char(' ');
	}
	write_char('\n');
	print_string("LED-set Configuration:\n");
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

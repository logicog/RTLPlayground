/*
 * This is a driver implementation for the Port features for the RTL827x platform
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"

#pragma codeseg BANK1

extern __code uint16_t bit_mask[16];
extern __xdata uint8_t minPort;
extern __xdata uint8_t maxPort;
extern __xdata uint8_t nSFPPorts;
extern __xdata uint8_t sfr_data[4];

void port_stats_print(void) __banked
{
	print_string("\r\n Port\tState\tLink\tTxGood\t\tTxBad\t\tRxGood\t\tRxBad\r\n");
	for (uint8_t i = minPort; i <= maxPort; i++) {
		write_char('1' + i); write_char('\t');
		phy_read(i, 0x1f, 0xa610); // p001f.a610:2058
		if (i <= maxPort - nSFPPorts) {
			if (SFR_DATA_8 == 0x20)
				print_string("On\t");
			else
				print_string("Off\t");
			reg_read_m(RTL837X_REG_LINKS);
			uint8_t b = sfr_data[3 - (i >> 1)];
			b = (i & 1) ? b >> 4 : b & 0xf;
			switch (b) {
			case 0:
				print_string("Down\t");
				break;
			case 1:
				print_string("100M\t");
				break;
			case 2:
				print_string("1000M\t");
				break;
			case 5:
				print_string("2.5G\t");
				break;
			default:
				print_string("Up\t");
				break;
			}
		} else {  // An SFP Module TODO: This is for 1 module devices
			reg_read_m(RTL837X_REG_GPIO_B);
			if (!(sfr_data[0] & 0x40)) {
				print_string("SFP OK\t");
			} else {
				print_string("NO SFP\t");
			}
			reg_read_m(RTL837X_REG_GPIO_C);
			if (sfr_data[3] & 0x20) {
				print_string("Down\t");
			} else {
				uint8_t rate = sfp_read_reg(12);
				if (rate == 0xd)
					print_string("1000BX\t");
				else if (rate == 0x1f)
					print_string("2500G\t");
				else if (rate > 0x65 && rate < 0x70)
					print_string("10G\t");
				else
					print_string("Up\t");
			}
		}
		// R0f60-000005e1 r0f60:000005e0 r0f64:00000000 r0f68:000000ba r1250:0fffffff R0f60-00000601 r0f60:00000600 r0f64:00000000 r0f68:00000000
		// R0f60-000005c1 r0f60:000005c0 r0f64:00000000 r0f68:000016dd R0f60-00000601 r0f60:00000600 r0f64:00000000 r0f68:00000000
		REG_WRITE(RTL837X_STAT_GET, 0x00, 0x00, 0x05, 0xe0 | (i << 1) | 1);
		do {
			reg_read_m(RTL837X_STAT_GET);
		} while (sfr_data[3] & 0x1);
		// FIXME: Ignore HIGHER part of 64 bit value for now
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');
		REG_WRITE(RTL837X_STAT_GET, 0x00, 0x00, 0x06, (i << 1) | 1);
		do {
			reg_read_m(RTL837X_STAT_GET);
		} while (sfr_data[3] & 0x1);
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');
		REG_WRITE(RTL837X_STAT_GET, 0x00, 0x00, 0x05, 0xc0 | (i << 1) | 1);
		do {
			reg_read_m(RTL837X_STAT_GET);
		} while (sfr_data[3] & 0x1);
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');
		REG_WRITE(RTL837X_STAT_GET, 0x00, 0x00, 0x06, (i << 1) | 1);
		do {
			reg_read_m(RTL837X_STAT_GET);
		} while (sfr_data[3] & 0x1);
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');
		print_string("\r\n");
	}
}

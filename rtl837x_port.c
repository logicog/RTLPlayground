/*
 * This is a driver implementation for the Port features for the RTL827x platform
 * This code is in the Public Domain
 */

// #define REGDBG

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

__xdata	uint32_t l2_head;

/*
 * Table access registers of the RTL837x
 * See e.g. RTL8366/RTL8369 datasheet for explanation
 */
#define RTL837X_TBL_CTRL	0x5cac
/* Bytes in control register: EE EE TT CC: EE: Entry, TT: Table type, CC: Command
 * CC: BIT 0: 01: Execute. Bit 1: 1: WRITE, 0: READ
 * TT: 04: L2-table, 03: VLAN-table
 */
#define TBL_L2_UNICAST 0x04

#define RTL837x_TBL_DATA_0	0x5cb0
#define RTL837x_L2_LIST_DATA_A	0x5ccc
#define RTL837x_L2_LIST_DATA_B	0x5cd0
#define RTL837x_L2_LIST_DATA_C	0x5cd4
#define RTL837x_TBL_DATA_IN_A	0x5cb8

#define RTL837x_PVID_BASE_REG	0x4e1c

void vlan_setup(void) __banked
{
	print_string("\r\nvlan_setup called \r\n");

	// Initialize VLAN table with VLAN 1
	REG_SET(RTL837x_TBL_DATA_IN_A, 0x0007ffff);
	REG_SET(RTL837X_TBL_CTRL, 0x00010303);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & 0x01);

	// Set PVID 1 for every port. TODO: Skip unused ports!
	for (uint8_t i =  minPort; i <= maxPort + 1; i++) {  // Do this also for the CPU port
		print_byte(i); write_char(':'); write_char(' ');
		uint16_t reg = RTL837x_PVID_BASE_REG + ((i >> 1) << 2);
		print_short(reg);  write_char(' '); write_char('B'); write_char('>');
		print_sfr_data();
		reg_read_m(reg);
		if (i & 0x1) {
			REG_WRITE(reg, sfr_data[0], 0, sfr_data[2] & 0x0f | 0x10, sfr_data[3]);
		} else {
			REG_WRITE(reg, sfr_data[0], sfr_data[1], sfr_data[2] & 0xf0, 0x01);
		}
		write_char(' '); write_char('A'); write_char('>'); print_sfr_data();

		reg_bit_clear(0x6738, i << 1);
		reg_bit_clear(0x6738, i << 1 + 1);
		reg_bit_set(0x4e18, i);
		print_string("\r\n");
	}

	// Enable 4k VLAN
	REG_SET(0x4e14, 4);
	REG_SET(0x4e30, 0);
	REG_SET(0x4e34, 0);

	// Enable VLAN 1
	REG_SET(RTL837x_TBL_DATA_IN_A, 0x0207ffff);
	REG_SET(RTL837X_TBL_CTRL, 0x00010303);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & 0x01);

	print_string("\r\nvlan_setup done \r\n");
}

/*
 * Forget all dynamic L2 learned entries
 */
uint8_t port_l2_forget(void) __banked
{
	// r53dc:00000000 R53dc-00000000 r53d4:000001ff r53d4:000001ff R53d4-000101ff r53d4:000001ff
	reg_read_m(0x53dc);
	if (sfr_data[0] || sfr_data[1] ||sfr_data[2] ||sfr_data[3]) {
		print_string("List busy\r\n");
		return -1;
	}
	REG_WRITE(0x53dc, sfr_data[0], sfr_data[1], sfr_data[2], sfr_data[3]);

	reg_read_m(0x53d4);
	REG_WRITE(0x53d4, 0x00, 0x01, sfr_data[2], sfr_data[3]);
	do {
		reg_read_m(0x53d4);
	} while (sfr_data[1] & 0x1);

	return 0;
}


void port_l2_learned(void) __banked
{
	// Whait for any table action to be finished
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & 0x01);
	print_string("\r\n\tMAC\t\tVLAN\ttype\tport\r\n");
	uint16_t entry = 0x0000;
	uint16_t first_entry = 0xffff; // Table does not have that many entries

	while (1) {
		uint8_t port = 0;
		reg_read_m(RTL837x_TBL_DATA_0);
		REG_WRITE(RTL837x_TBL_DATA_0, sfr_data[0], sfr_data[1],sfr_data[2] | 0xc0, sfr_data[3]);

		REG_WRITE(RTL837X_TBL_CTRL, entry >> 8, entry, TBL_L2_UNICAST, 0x1);
		do {
			reg_read_m(RTL837X_TBL_CTRL);
		} while (sfr_data[3] & 0x1);

		// MAC
		reg_read_m(RTL837x_L2_LIST_DATA_B);
		print_byte(sfr_data[2]); write_char(':');
		print_byte(sfr_data[3]); write_char(':');
		port = (sfr_data[0] >> 6) & 0x3;
		reg_read_m(RTL837x_L2_LIST_DATA_A);
		print_byte(sfr_data[0]); write_char(':');
		print_byte(sfr_data[1]); write_char(':');
		print_byte(sfr_data[2]); write_char(':');
		print_byte(sfr_data[3]); write_char('\t');

		// VLAN
		reg_read_m(RTL837x_L2_LIST_DATA_B);
		print_short( ((uint16_t) (sfr_data[0] & 0x0f)) | sfr_data[1]); // VLAN

		// type
		reg_read_m(RTL837x_L2_LIST_DATA_C);
		if (sfr_data[2] & 0x1)
			print_string("\tstatic\t");
		else
			print_string("\tlearned\t");

		port |= (sfr_data[3] & 0x3) << 2;
		if (port < 9)
			write_char('1' + port);
		else
			print_string("10");

		reg_read_m(RTL837x_TBL_DATA_0);
		entry = (((uint16_t)sfr_data[2] & 0x0f) << 8) | sfr_data[3] + 1;
		if (first_entry == 0xffff) {
			first_entry = entry;
		} else {
			if (first_entry == entry)
				break;
		}
		print_string("\r\n");
	}
	print_string("\r\nport_l2_learned done \r\n");
}


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

/*
 * This is a driver implementation for the Port features for the RTL827x platform
 * This code is in the Public Domain
 */

#define REGDBG
#define DEBUG

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
extern __xdata uint8_t cpuPort;

__xdata	uint32_t l2_head;


void port_mirror_set(register uint8_t port, __xdata uint16_t rx_pmask, __xdata uint16_t tx_pmask) __banked
{
	print_string("\r\nport_mirror_set called \r\n");

	REG_WRITE(RTL837x_MIRROR_CONF, rx_pmask >> 8, rx_pmask, tx_pmask >> 8, tx_pmask);
	REG_WRITE(RTL837x_MIRROR_CTRL, 0, 0, 0, (port << 1) | 0x1);
}


void port_mirror_del(void) __banked
{
	print_string("\r\nport_mirror_del called \r\n");
	REG_SET(RTL837x_MIRROR_CTRL, 0);
}


void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked
{
	// r4e1c:00001001 R4e1c-000017d0 r6738:00000000 R6738-00000000 (no filtering)
	print_string("\r\nport_pvid_set called \r\n");
	uint16_t reg = RTL837x_PVID_BASE_REG + ((port >> 1) << 2);

	reg_read_m(reg);
	if (port & 0x1) {
		REG_WRITE(reg, sfr_data[0], pvid >> 4, sfr_data[2] & 0x0f | (pvid << 4), sfr_data[3]);
	} else {
		REG_WRITE(reg, sfr_data[0], sfr_data[1], sfr_data[2] & 0xf0 | (pvid >> 8), pvid);
	}
}


void vlan_delete(uint16_t vlan) __banked
{
	print_string("\r\nvlan_delete called \r\n"); print_short(vlan);
	// R5cac-07d30301 r5cac:07d30300
}


/*
 * A member that is not tagged, is untagged
 */
void vlan_create(register uint16_t vlan, register uint16_t members, register uint16_t tagged) __banked
{
	/* First line:
		7-9: Untagged: 1-1, Not-Member: 1-0
		0111111111
		1111000000
		9  port  0

		1-3: Untagged: 1-1
		0111111111
		1000000111

		1-3: Tagged: 0-1
		0111111000
		1000000111

		7-9: Tagged: 0-1
		0000111111
		1111000000
		In 1-0 -> 1-1   FIRST Bit: XOR, Second bit stays
		In 1-1 -> 0-1
		In 0-1 // Not allowed
		In 0-0 -> 1-0
	*/
	// R5cb8-02 07e207 R5cac-07d20303
	print_string("\r\nvlan_create called: "); print_short(vlan); write_char(' '); print_short(members); write_char(':'); print_short(tagged);

	uint16_t a = members ^ tagged;
	// Initialize VLAN table with VLAN 1
	REG_WRITE(RTL837x_TBL_DATA_IN_A, 0x02, (a >> 6) & 0x0f, (a << 2) | (tagged >> 8), tagged);
	REG_WRITE(RTL837X_TBL_CTRL, vlan >> 8, vlan, TBL_VLAN, TBL_WRITE | TBL_EXECUTE);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);
	print_string("\r\nvlan_create done \r\n");
}


/*
 * Configures a default VLAN 1 and enables 4k VLAN tables
 * All ports are made members of the VLAN and VLAN filtering
 * is enabled on all ports
 * PVID is set to 1 for all ports
 * Called upon reboot
 */
void vlan_setup(void) __banked
{
	print_string("\r\nvlan_setup called \r\n");

	// Initialize VLAN table with VLAN 1
	REG_SET(RTL837x_TBL_DATA_IN_A, 0x0007ffff);
	REG_SET(RTL837X_TBL_CTRL, 0x00010303);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);

	// Set PVID 1 for every port. TODO: Skip unused ports!
	for (uint8_t i =  minPort; i <= maxPort + 1; i++) {  // Do this also for the CPU port (+1)
		uint16_t reg = RTL837x_PVID_BASE_REG + ((i >> 1) << 2);
#ifdef DEBUG
		print_byte(i); write_char(':'); write_char(' ');
		print_short(reg);  write_char(' '); write_char('B'); write_char('>');
		print_sfr_data();
#endif
		reg_read_m(reg);
		if (i & 0x1) {
			REG_WRITE(reg, sfr_data[0], 0, sfr_data[2] & 0x0f | 0x10, sfr_data[3]);
		} else {
			REG_WRITE(reg, sfr_data[0], sfr_data[1], sfr_data[2] & 0xf0, 0x01);
		}
#ifdef DEBUG
		reg_read_m(reg);
		write_char(' '); write_char('A'); write_char('>'); print_sfr_data();
#endif

		// EGRESS filtering for port: removal of additional VLAN tag
		reg_bit_clear(0x6738, i << 1);
		reg_bit_clear(0x6738, (i << 1) + 1);
		if (i != cpuPort)
			reg_bit_set(0x4e18, i);
		else
			reg_bit_clear(0x4e18, i);
#ifdef DEBUG
		print_string("\r\n");
#endif
	}
	// Enable 4k VLAN
	REG_SET(0x4e14, 4);
	REG_SET(0x4e30, 0);
	REG_SET(0x4e34, 0);

	// Enable VLAN 1: Ports 0-9, i.e. including the CPU port are untagged members
	REG_SET(RTL837x_TBL_DATA_IN_A, 0x0207ffff);		// 02: Entry valid, 7ffff: membership
	REG_SET(RTL837X_TBL_CTRL, 0x00010303);	// Write VLAN 1
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);

	// Configure trunking
	REG_SET(0x4f4c, 0x0000007e);

#ifdef DEBUG
	print_string("\r\nvlan_setup, REG 0x6738: "); print_reg(0x6738);
	print_string("\r\nvlan_setup, REG 0x4e18: "); print_reg(0x4e18);
	print_string("\r\nvlan_setup, REG 0x4e14: "); print_reg(0x4e14);
	print_string("\r\nvlan_setup, REG 0x4e30: "); print_reg(0x4e30);
	print_string("\r\nvlan_setup, REG 0x4e34: "); print_reg(0x4e34);
	print_string("\r\nvlan_setup, REG 0x4f4c: "); print_reg(0x4f4c);
#endif

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

	reg_read_m(RTL837x_L2_TBL_CTRL);
	REG_WRITE(RTL837x_L2_TBL_CTRL, 0x00, 0x01, sfr_data[2], sfr_data[3]);
	do {
		reg_read_m(RTL837x_L2_TBL_CTRL);
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
	__xdata uint16_t entry = 0x0000;
	__xdata uint16_t first_entry = 0xffff; // Table does not have that many entries

	while (1) {
		uint8_t port = 0, other = 0;
		reg_read_m(RTL837x_TBL_DATA_0);
		REG_WRITE(RTL837x_TBL_DATA_0, sfr_data[0], sfr_data[1],sfr_data[2] | 0xc0, sfr_data[3]);

		REG_WRITE(RTL837X_TBL_CTRL, entry >> 8, entry, TBL_L2_UNICAST, 0x1);
		do {
			reg_read_m(RTL837X_TBL_CTRL);
		} while (sfr_data[3] & 0x1);

		// MAC
		reg_read_m(RTL837x_L2_DATA_OUT_B);
		if ((sfr_data[0] & 0x20)) {	// Check entry is valid
			print_byte(sfr_data[2]); write_char(':');
			print_byte(sfr_data[3]); write_char(':');
			port = (sfr_data[0] >> 6) & 0x3;
			other = sfr_data[0];
			reg_read_m(RTL837x_L2_DATA_OUT_A);
			print_byte(sfr_data[0]); write_char(':');
			print_byte(sfr_data[1]); write_char(':');
			print_byte(sfr_data[2]); write_char(':');
			print_byte(sfr_data[3]); write_char('\t');

			// VLAN
			reg_read_m(RTL837x_L2_DATA_OUT_B);
			print_short( ((uint16_t) (sfr_data[0] & 0x0f)) | sfr_data[1]); // VLAN

			// type
			reg_read_m(RTL837x_L2_DATA_OUT_C);
			if (sfr_data[2] & 0x1)
				print_string("\tstatic\t");
			else
				print_string("\tlearned\t");

			port |= (sfr_data[3] & 0x3) << 2;
			if (port < 9)
				write_char('1' + port);
			else
				print_string("10");
		}
		reg_read_m(RTL837x_TBL_DATA_0);
		entry = (((uint16_t)sfr_data[2] & 0x0f) << 8) | sfr_data[3] + 1;
		if (first_entry == 0xffff) {
			first_entry = entry;
		} else {
			if (first_entry == entry)
				break;
		}
#ifdef DEBUG
		write_char(' '); print_sfr_data();
		write_char(' '); print_byte(other);
#endif
		print_string("\r\n");
	}
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
				uint8_t rate = sfp_read_reg(0, 12);
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

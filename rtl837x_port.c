/*
 * This is a driver implementation for the Port features for the RTL827x platform
 * This code is in the Public Domain
 */

// #define REGDBG
// #define DEBUG

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_port.h"

#pragma codeseg BANK1

extern __code uint8_t * __code hex;
extern __code uint16_t bit_mask[16];
extern __xdata uint8_t minPort;
extern __xdata uint8_t maxPort;
extern __xdata uint8_t nSFPPorts;
extern __xdata uint8_t sfr_data[4];
extern __xdata uint8_t cpuPort;
extern __xdata uint16_t vlan_ptr;
extern __xdata uint8_t vlan_names[VLAN_NAMES_SIZE];

extern __xdata uint8_t isRTL8373;

__xdata	uint32_t l2_head;

// The mapping of logical to physical ports on the RTL8372
// Port 6 is always an SFP+ port. Port 5 may be RTL8221 or SFP+
__code uint8_t log_to_phys_port[9] = {
	0, 0, 0, 5, 1, 2, 3, 4, 6
};

#if NSFP == 2
__code uint8_t is_sfp[9] = {
	0, 0, 0, 1, 0, 0, 0, 0, 1
};
#else
__code uint8_t is_sfp[9] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1
};
#endif

void port_mirror_set(register uint8_t port, __xdata uint16_t rx_pmask, __xdata uint16_t tx_pmask) __banked
{
	print_string("\nport_mirror_set called \n");
	print_string("Mirroring port: "); print_byte(port); print_string(" with rx-mask: ");
	print_short(rx_pmask); print_string(", tx mask: "); print_short(tx_pmask);

	REG_WRITE(RTL837x_MIRROR_CONF, rx_pmask >> 8, rx_pmask, tx_pmask >> 8, tx_pmask);
	REG_WRITE(RTL837x_MIRROR_CTRL, 0, 0, 0, (port << 1) | 0x1);
}


void port_mirror_del(void) __banked
{
	print_string("\nport_mirror_del called \n");
	REG_SET(RTL837x_MIRROR_CTRL, 0);
}


void port_ingress_filter(register uint8_t port, uint8_t type) __banked
{
	if (type & 0x1)
		reg_bit_set(RTL837x_REG_INGRESS, port << 1);
	else
		reg_bit_clear(RTL837x_REG_INGRESS, port << 1);
	if (type & 0x2)
		reg_bit_set(RTL837x_REG_INGRESS, (port << 1) + 1);
	else
		reg_bit_clear(RTL837x_REG_INGRESS, (port << 1) + 1);
}


/*
 * Define a Primary VLAN ID for a port 
*/
void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked
{
	// r4e1c:00001001 R4e1c-000017d0 r6738:00000000 R6738-00000000 (no filtering)
	print_string("\nport_pvid_set called \n");
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
	print_string("\nvlan_delete called \n"); print_short(vlan);
	REG_WRITE(RTL837x_TBL_DATA_IN_A, 0, 0, 0, 0);
	REG_WRITE(RTL837X_TBL_CTRL, vlan >> 8, vlan, TBL_VLAN, TBL_WRITE | TBL_EXECUTE);
}


/*
 * Reads VLAN information from VLAN table
 * Returns data in sfr_data
 */
int8_t vlan_get(register uint16_t vlan) __banked
{
	if (vlan >= 0x3ff) // VLAN 4095 is special
		return -1;

	REG_WRITE(RTL837X_TBL_CTRL, vlan >> 8, vlan, TBL_VLAN, TBL_EXECUTE);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);
	reg_read_m(RTL837x_L2_DATA_OUT_A);

	return 0;
}


__xdata uint16_t vlan_name(register uint16_t vlan) __banked
{
	__xdata int16_t i = 0;
	__xdata uint8_t begin = 1;
	while (vlan_names[i]) {
		if (begin && vlan_names[i] == hex[(vlan >> 8) & 0xf] && vlan_names[i + 1] == hex[(vlan >> 4) & 0xf] && vlan_names[i + 2] == hex[vlan & 0xf])
			break;
		begin = vlan_names[i++] == ' ' ? 1 : 0;
	}
	if (vlan_names[i])
		return i + 3;

	return 0xffff;
}


/*
 * A member that is not tagged, is untagged
 */
void vlan_create(register uint16_t vlan, register uint16_t members, register uint16_t tagged) __banked
{
	// For now, the CPU-port is always a tagged member:
	members |= 0x0200; // Set 10th bit
	tagged |= 0x0200;
	print_string("\nvlan_create called\nvlan: "); print_short(vlan);
	print_string(", members: "); print_short(members);
	print_string(", tagged: "); print_short(tagged); write_char('\n');

	uint16_t a = (~members) ^ tagged ^ members;
	// On RTL8372, port-bits 0-2 must be 0, although they are not members
	if (!isRTL8373) {
		a &= 0x1f8;
		tagged &= 0x3f8;
	}

	// Initialize VLAN table with VLAN 1
	REG_WRITE(RTL837x_TBL_DATA_IN_A, 0x02, (a >> 6) & 0x0f, (a << 2) | (members >> 8), members);
	REG_WRITE(RTL837X_TBL_CTRL, vlan >> 8, vlan, TBL_VLAN, TBL_WRITE | TBL_EXECUTE);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);
	print_string("vlan_create done \n");
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
	print_string("\nvlan_setup called \n");

	// No VLAN names set up so far
	vlan_ptr = 0;
	vlan_names[0] = 0;

	// Initialize VLAN table for VLAN 1, by disabling that entry
	if (isRTL8373) {
		REG_SET(RTL837x_TBL_DATA_IN_A, 0x0007ffff);
	} else {
		REG_SET(RTL837x_TBL_DATA_IN_A, 0x0007e3f8);
	}
	REG_SET(RTL837X_TBL_CTRL, 0x00010303);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);

	// Set PVID 1 for every port. TODO: Skip unused ports!
	for (uint8_t i = minPort; i <= maxPort + 1; i++) {  // Do this also for the CPU port (+1)
		uint16_t reg = RTL837x_PVID_BASE_REG + ((i >> 1) << 2);
#ifdef DEBUG
		print_byte(i); write_char(':'); write_char(' '); print_short(reg); write_char('=');
		print_short(reg);  write_char(' ');
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
		reg_bit_set(0x4e18, i);

#ifdef DEBUG
		print_string("\n");
#endif
	}

	// Ingress filtering. 2 bits per port: allow tagged (01) / untagged (10) and all (00)
	REG_SET(RTL837x_REG_INGRESS, 0); // No filtering for all ports

	// Enable 4k VLAN
	REG_SET(0x4e14, 4);
	REG_SET(0x4e30, 0);
	REG_SET(0x4e34, 0);

	// Enable VLAN 1: Ports 0-9, i.e. including the CPU port are untagged members
	if (isRTL8373) {
		REG_SET(RTL837x_TBL_DATA_IN_A, 0x0207ffff);	// 02: Entry valid, 7ffff: membership
	} else {
		REG_SET(RTL837x_TBL_DATA_IN_A, 0x0207e3f8);
	}
	REG_SET(RTL837X_TBL_CTRL, 0x00010303);	// Write VLAN 1
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);

	// Configure trunking
	if (isRTL8373) {
		REG_SET(0x4f4c, 0x0000007e); // Removes RTL VLAN-Tags
		REG_SET(0x4f48, 0x0000007e); // Adds 802.1Q VLAN-Tags to tagged ports
	}

#ifdef DEBUG
	print_string("\nvlan_setup, REG 0x6738: "); print_reg(0x6738);
	print_string("\nvlan_setup, REG 0x4e18: "); print_reg(0x4e18);
	print_string("\nvlan_setup, REG 0x4e14: "); print_reg(0x4e14);
	print_string("\nvlan_setup, REG 0x4e30: "); print_reg(0x4e30);
	print_string("\nvlan_setup, REG 0x4e34: "); print_reg(0x4e34);
	print_string("\nvlan_setup, REG 0x4f48: "); print_reg(0x4f48);
	print_string("\nvlan_setup, REG 0x4f4c: "); print_reg(0x4f4c);
#endif

	print_string("vlan_setup done \n");
}


void trunk_set(uint8_t group, uint16_t mask) __banked
{
	if (group == 1) {
		REG_WRITE(RTL837x_TRUNK_CTRL_A, 0, 0, mask >> 8, mask);
	} else if (group == 2) {
		REG_WRITE(RTL837x_TRUNK_CTRL_B, 0, 0, mask >> 8, mask);
	} else {
		print_string("\nTrunk group must be 1 or 2\n");
	}
}


/*
 * Forget all dynamic L2 learned entries
 */
uint8_t port_l2_forget(void) __banked
{
	print_string("\nport_l2_forget called\n");
	// Configure the entries to be flushed:
	// port-based (bits 0-1 are 0 and dynamic entries, bit 2 specifies dynamic entries
	REG_SET(RTL837x_L2_TBL_FLUSH_CNF, 0x0);

	// Flush L2 table for all ports by setting the ports and the flush-exec bit (bit 16)
	if (isRTL8373) {
		REG_SET(RTL837x_L2_TBL_FLUSH_CTRL, L2_TBL_FLUSH_EXEC | PMASK_9);
	} else {
		REG_SET(RTL837x_L2_TBL_FLUSH_CTRL, L2_TBL_FLUSH_EXEC | PMASK_6);
	}

	// Wait for flush completed
	do {
		reg_read_m(RTL837x_L2_TBL_FLUSH_CTRL);
	} while (sfr_data[1]);

	print_string("port_l2_forget done\n");
	return 0;
}


void port_l2_learned(void) __banked
{
	// Whait for any table action to be finished
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & 0x01);
	print_string("\n\tMAC\t\tVLAN\ttype\tport\n");
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
		print_string("\n");
	}
}


/*
 * Basic L2 configuration such as time to forget an entry
 */
void port_l2_setup() __banked
{
	print_string("\nport_l2_setup called\n");

	port_l2_forget();

	for (uint8_t i = minPort; i <= maxPort; i++) {
		uint16_t reg = 0x5384 + (i << 2);
		REG_SET(reg, 0x00001040);

		// All ports may communicate with each other and CPU-Port
		reg = RTL837X_PORT_ISOLATION_BASE + (i << 2);
		if(isRTL8373) {
			REG_SET(reg, PMASK_9 | PMASK_CPU);
		} else {
			REG_SET(reg, PMASK_6 | PMASK_CPU);
		}
	}
	reg_bit_set(0x4f80, 0);

	print_string("\nport_l2_setup done\n");
}


void port_stats_print(void) __banked
{
	print_string("\n Port\tState\tLink\tTxGood\t\tTxBad\t\tRxGood\t\tRxBad\n");
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
		STAT_GET(0x2f, i);
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');

		STAT_GET(0x30, i);
		print_reg(RTL837X_STAT_V_HIGH); write_char('\t');

		STAT_GET(0x2e, i);
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');

		STAT_GET(0x30, i);
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');
		print_string("\n");
	}
}


void port_isolate(register uint8_t port, __xdata uint16_t pmask)
{
	if (port <= maxPort)
		REG_SET(RTL837X_PORT_ISOLATION_BASE + (port << 2), pmask);
}


uint16_t port_isolation_get(register uint8_t port)
{
	if (port > maxPort)
		return 0;

	reg_read_m(RTL837X_PORT_ISOLATION_BASE + (port << 2));
	return ((uint16_t)sfr_data[2]) << 8 | sfr_data[3];
}

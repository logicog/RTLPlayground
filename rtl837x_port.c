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
#include "rtl837x_phy.h"
#include "phy.h"
#include "machine.h"

#pragma codeseg BANK1
#pragma constseg BANK1

extern __code uint8_t * __code hex;
extern __code uint16_t bit_mask[16];
extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata uint16_t vlan_ptr;
extern __xdata uint8_t vlan_names[VLAN_NAMES_SIZE];

__xdata	uint32_t l2_head;

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
	if (!machine.isRTL8373) {
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
	REG_SET(RTL837x_TBL_DATA_IN_A, machine.isRTL8373? 0x0007ffff : 0x0007e3f8);

	REG_SET(RTL837X_TBL_CTRL, 0x00010303);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);

	// Set PVID 1 for every port. TODO: Skip unused ports!
	for (uint8_t i = machine.min_port; i <= machine.max_port + 1; i++) {  // Do this also for the CPU port (+1)
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

		// EGRESS filtering for port: removal of additional VLAN tag (mode 0x3 for each port)
		reg_bit_clear(RTL837X_VLAN_PORT_EGR_TAG, i << 1);
		reg_bit_clear(RTL837X_VLAN_PORT_EGR_TAG, (i << 1) + 1);
		reg_bit_set(RTL837X_VLAN_PORT_IGR_FLTR, i);

#ifdef DEBUG
		print_string("\n");
#endif
	}

	// Ingress filtering. 2 bits per port: allow tagged (01) / untagged (10) and all (00)
	REG_SET(RTL837x_REG_INGRESS, 0); // No filtering for all ports

	// Enable 4k VLAN
	REG_SET(RTL837X_VLAN_CTRL, VLAN_CVLAN_FILTER);
	REG_SET(RTL837X_VLAN_L2_LRN_DIS_0, 0);
	REG_SET(RTL837X_VLAN_L2_LRN_DIS_1, 0);

	// Enable VLAN 1: Ports 0-9, i.e. including the CPU port are untagged members
	REG_SET(RTL837x_TBL_DATA_IN_A, machine.isRTL8373? 0x0207ffff : 0x0207e3f8);	// 02: Entry valid, 7...: membership

	REG_SET(RTL837X_TBL_CTRL, 0x00010303);	// Write VLAN 1
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & TBL_EXECUTE);

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
	REG_SET(RTL837x_L2_TBL_FLUSH_CTRL, L2_TBL_FLUSH_EXEC | (machine.isRTL8373 ? PMASK_9 : PMASK_6));

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
		uint8_t port = 0;
		reg_read_m(RTL837x_TBL_DATA_0);
		REG_WRITE(RTL837x_TBL_DATA_0, sfr_data[0], sfr_data[1],sfr_data[2] | 0xc0, sfr_data[3]);

		REG_WRITE(RTL837X_TBL_CTRL, (entry >> 8) & 0xf, entry, TBL_L2_UNICAST, TBL_EXECUTE);
		do {
			reg_read_m(RTL837X_TBL_CTRL);
		} while (sfr_data[3] & TBL_EXECUTE);

		reg_read_m(RTL837x_TBL_DATA_0);
		entry = (((uint16_t)sfr_data[2] & 0x0f) << 8) | sfr_data[3];
		if (first_entry == 0xffff) {
			first_entry = entry;
		} else {
			if (first_entry == entry)
				break;
		}

		// MAC
		reg_read_m(RTL837x_L2_DATA_OUT_B);
		if ((sfr_data[0] & 0x20)) {	// Check entry is valid
			print_byte(sfr_data[2]); write_char(':');
			print_byte(sfr_data[3]); write_char(':');
			port = (sfr_data[0] >> 6) & 0x3;
			reg_read_m(RTL837x_L2_DATA_OUT_A);
			print_byte(sfr_data[0]); write_char(':');
			print_byte(sfr_data[1]); write_char(':');
			print_byte(sfr_data[2]); write_char(':');
			print_byte(sfr_data[3]); write_char('\t');

			// VLAN
			reg_read_m(RTL837x_L2_DATA_OUT_B);
			print_short( (((uint16_t) (sfr_data[0] & 0x0f)) << 8) | sfr_data[1]); // VLAN

			// type
			reg_read_m(RTL837x_L2_DATA_OUT_C);
			if (sfr_data[2] & 0x1)
				print_string("\tstatic\t");
			else
				print_string("\tlearned\t");

			port |= (sfr_data[3] & 0x3) << 2;
			if (port < 9)
				write_char(machine.log_to_phys_port[port] + '0');
			else
				print_string("CPU");
		}

		entry++;
		print_string("\n");
	}
}


/*
 * Basic L2 configuration such as time to forget an entry
 */
void port_l2_setup(void) __banked
{
	print_string("\nport_l2_setup called\n");

	port_l2_forget();

	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		// Limit the number of automatically learned MAC-Entries per port to 0x1040
		uint16_t reg = RTL837X_L2_LRN_PORT_CONSTRAINT + (i << 2);
		REG_SET(reg, 0x00001040);

		// All ports may communicate with each other and CPU-Port
		reg = RTL837X_PORT_ISOLATION_BASE + (i << 2);
		REG_SET(reg, PMASK_CPU | (machine.isRTL8373? PMASK_9 : PMASK_6));
	}
	// When maximim entries learned, then simply flood the packet
	reg_bit_set(RTL837X_L2_LRN_PORT_CONSTRT_ACT, 0);

	print_string("\nport_l2_setup done\n");
}


void port_stats_print(void) __banked
{
	print_string("\n Port\tState\tLink\tTxGood\t\tTxBad\t\tRxGood\t\tRxBad\n");
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		write_char('0' + machine.log_to_phys_port[i]); write_char('\t');

		if (!machine.is_sfp[i]) {
			phy_read(i, 0x1f, 0xa610);
			if (SFR_DATA_8 == 0x20)
				print_string("On\t");
			else
				print_string("Off\t");
		} else {  // An SFP Module
			if (i != 3) {
				reg_read_m(RTL837X_REG_GPIO_00_31_INPUT);
				if (!(sfr_data[0] & 0x40)) {
					print_string("SFP OK\t");
				} else {
					print_string("NO SFP\t");
				}
			} else {
				reg_read_m(RTL837X_REG_GPIO_32_63_INPUT);
				if (!(sfr_data[1] & 0x04)) {
					print_string("SFP OK\t");
				} else {
					print_string("NO SFP\t");
				}
			}
		}

		if (i < 8)
			reg_read_m(RTL837X_REG_LINKS);
		else
			reg_read_m(RTL837X_REG_LINKS_89);
		uint8_t b = sfr_data[3 - ((i & 7) >> 1)];
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
		case 4:
			print_string("10G\t");
			break;
		case 5:
			print_string("2.5G\t");
			break;
		default:
			print_string("Up\t");
			break;
		}
		STAT_GET(STAT_COUNTER_TX_PKTS, i);
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');

		STAT_GET(STAT_COUNTER_ERR_PKTS, i);
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');

		STAT_GET(STAT_COUNTER_RX_PKTS, i);
		print_reg(RTL837X_STAT_V_LOW); write_char('\t');

		STAT_GET(STAT_COUNTER_ERR_PKTS, i);
		print_reg(RTL837X_STAT_V_HIGH); write_char('\t');
		print_string("\n");
	}
}


void port_isolate(register uint8_t port, __xdata uint16_t pmask)
{
	if (port <= machine.max_port)
		REG_SET(RTL837X_PORT_ISOLATION_BASE + (port << 2), pmask);
}


uint16_t port_isolation_get(register uint8_t port)
{
	if (port > machine.max_port)
		return 0;

	reg_read_m(RTL837X_PORT_ISOLATION_BASE + (port << 2));
	return ((uint16_t)sfr_data[2]) << 8 | sfr_data[3];
}


void port_eee_enable(uint8_t port) __banked
{
	if (machine.is_sfp[port])
		return;

	REG_SET(RTL8373_EEE_CTRL_BASE + (port << 2), EEE_100 | EEE_1000 | EEE_2G5);
	// Enable EEE advertisement for 100/1000BASE-T via EEE Advertisement Reg
	phy_write(port, PHY_MMD_AN, PHY_EEE_ADV, PHY_EEE_BIT_1G | PHY_EEE_BIT_100M);
	// Enable EEE advertisement for 2.5GBASE-T via EEE Advertisement Reg 2
	phy_write(port, PHY_MMD_AN, PHY_EEE_ADV2, PHY_EEE_BIT_2G5);
	phy_reset(port);
}


void port_eee_disable(uint8_t port) __banked
{
	if (machine.is_sfp[port])
		return;

	print_string("EEE off for "); print_byte(port); write_char('\n');
	REG_SET(RTL8373_EEE_CTRL_BASE + (port << 2), 0);
	// Disable EEE advertisement for 100/1000BASE-T via EEE Advertisement Reg
	phy_write(port, PHY_MMD_AN, PHY_EEE_ADV, 0);
	// Disable EEE advertisement for 2.5GBASE-T via EEE Advertisement Reg 2
	phy_write(port, PHY_MMD_AN, PHY_EEE_ADV2, 0);
	phy_reset(port);
}


void port_eee_status(uint8_t port) __banked
{
	print_string("Port: "); write_char('0' + machine.log_to_phys_port[port]);
	print_string(": ");
	if (machine.is_sfp[port]) {
		print_string("SFP\n");
		return;
	}

	uint16_t v;
	print_string("Advertising: ");
	phy_read(port, PHY_MMD_AN, PHY_EEE_ADV2);
	v = SFR_DATA_U16;
	if (v & PHY_EEE_BIT_2G5)
		print_string(" 2.5G");
	else
		print_string("     ");
	phy_read(port, PHY_MMD_AN, PHY_EEE_ADV);
	v = SFR_DATA_U16;
	if (v & PHY_EEE_BIT_1G)
		print_string("  1G ");
	else
		print_string("     ");
	if (v & PHY_EEE_BIT_100M)
		print_string(" 100M");
	else
		print_string("     ");

	print_string("   Link Partner: ");
	phy_read(port, PHY_MMD_AN, PHY_EEE_LP_ABILITY2);
	v = SFR_DATA_U16;
	if (v & PHY_EEE_BIT_2G5)
		print_string(" 2.5G");
	else
		print_string("     ");
	phy_read(port, PHY_MMD_AN, PHY_EEE_LP_ABILITY);
	v = SFR_DATA_U16;
	if (v & PHY_EEE_BIT_1G)
		print_string("  1G ");
	else
		print_string("     ");
	if (v & PHY_EEE_BIT_100M)
		print_string(" 100M");
	else
		print_string("     ");

	reg_read_m(RTL8373_PHY_EEE_ABLTY);
	if (sfr_data[3] & (1 << port))
		print_string(" ACTIVE   ");
	else
		print_string(" INACTIVE ");
	write_char('\n');
}


void port_eee_enable_all(void) __banked
{
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		port_eee_enable(i);
	}
}


void port_eee_disable_all(void) __banked
{
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		port_eee_disable(i);
	}
}


void port_eee_status_all(void) __banked
{
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		port_eee_status(i);
	}
}

/*
 * Enable RLDP, Realtek's version of LLDP
 */
void port_rldp_on(__xdata uint16_t p_ms)
{
	REG_WRITE(RTL8373_RLDP_TIMER, p_ms >> 8, p_ms, p_ms >> 8, p_ms);

	REG_SET(RTL837X_RMA0_CONF, 0x00000000); // R4ecc
	REG_SET(RTL837X_RMA_CONF, 0x00000000); // R4ecc
}


/*
 * Configure LAGs
 * Sets the members via port bitmask of a given Link Aggregation Group
 * The groups have numbers 0-3
 * The bitmask represents up to 10 ports
 * If currently no LAG has algorithm used, a default is applied
 */
void port_lag_members_set(__xdata uint8_t lag, __xdata uint16_t members) __banked
{
	print_string("port_lag_members_set, lag: "); print_byte(lag); print_string(", members: "); print_short(members);
	if (lag > 3)
		print_string("Link aggregation group must be 0-3!");
	reg_read_m(RTL837X_TRK_HASH_CTRL_BASE + (lag << 2));
	if (!(sfr_data[0] | sfr_data [1] | sfr_data [2] | sfr_data [3]))
		REG_SET(RTL837X_TRK_HASH_CTRL_BASE, LAG_HASH_DEFAULT);
	REG_WRITE(RTL837X_TRK_MBR_CTRL_BASE + (lag << 2), 0, 0, members >> 8, members & 0xff);
}


/*
 * Configures the hash algorithm used for a LAG
 * lag is the Group to configure and hash is a bitmask
 */
void port_lag_hash_set(__xdata uint8_t lag, __xdata uint8_t hash_bits) __banked
{
	print_string("port_lag_hash_set, lag: "); print_byte(lag); print_string(", hash: "); print_byte(hash_bits);
	if (lag > 3)
		print_string("Link aggregation group must be 0-3!");
	REG_WRITE(RTL837X_TRK_HASH_CTRL_BASE + (lag << 2), 0, 0, 0, hash_bits);
}

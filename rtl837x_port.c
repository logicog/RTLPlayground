/*
 * This is a driver implementation for the Port features for the RTL827x platform
 * This code is in the Public Domain
 */

// #define REGDBG
// #define DEBUG

#include <stdint.h>
#include <stdbool.h>
#include "rtl837x_common.h"
#include "rtlos_hal.h"
#include "rtl837x_sfr.h"
#include "rtlos_regs.h"
#include "rtl837x_flash.h"
#include "rtl837x_port.h"
#include "rtl837x_nic.h"
#include "rtlos_phy.h"
#include "phy.h"
#include "machine.h"

/*
 * Bank placement. BANK1 (this file's historical home, shared with
 * httpd/page_impl.c) is full on the RTL836x: the eight-port VLAN core and its
 * diagnostics leave only ~69 bytes there, which is not enough for any further
 * 802.1Q work. So the RTL836x build takes this file to BANK3, which has ~18 KB
 * spare (rtl836x_nic.c is its only other tenant), while every other machine
 * keeps the original BANK1 layout and therefore a byte-identical image.
 *
 * Every entry point in this file is __banked, so callers in HOME/BANK1/BANK2
 * are unaffected by the move. Internal statics are still called within the
 * same bank, and the file calls only HOME helpers across banks.
 */
#if SOC_FAMILY == RTL836X
#pragma codeseg BANK3
#pragma constseg BANK3
#else
#pragma codeseg BANK1
#pragma constseg BANK1
#endif

extern __code const uint8_t * __code const hex;
extern __code const uint16_t bit_mask[16];
extern __code const struct machine machine;
extern __xdata uint16_t vlan_ptr;
extern __xdata uint8_t vlan_names[VLAN_NAMES_SIZE];
extern __xdata struct machine_runtime machine_detected;

__xdata	uint32_t l2_head;

__xdata struct vlan_settings vlan_settings;

#if SOC_FAMILY == RTL836X

static __xdata uint16_t rtl836x_vlan_member;
static __xdata uint16_t rtl836x_vlan_untag;
static __xdata uint8_t rtl836x_vlan_fid;
static __xdata uint16_t rtl836x_pvid;
static __xdata uint16_t rtl836x_mc_member;
static __xdata uint8_t rtl836x_mc_fid;
static __xdata uint16_t rtl836x_vlan_all;

#define rtl836x_sfr_u16() ((uint16_t)(((uint16_t)sfr_data[2] << 8) | sfr_data[3]))

/* The table engine has no ready bit (see the note on wait_table_ready()), so
 * every 0x0500 transaction has to be followed by a wait. A design tick --
 * what rtl836x_tbl_settle() waits -- is orders of magnitude longer than the
 * engine's turnaround, which is fine for the handful of accesses boot makes,
 * but not for the loops that touch one entry per VID: there the wait has to
 * outlast the engine, not a scheduler tick, so burn a bounded number of bus
 * cycles instead. Used by the L2 walk and by the VLAN sweep of
 * send_vlanlist(). */
#define RTL836x_TBL_FAST_SETTLE_LOOPS	8

/*
static void rtl836x_tbl_settle_fast(void) __reentrant
{
	uint8_t i;

	for (i = 0; i < RTL836x_TBL_FAST_SETTLE_LOOPS; i++)
		reg_read(RTL836x_TA_LUT_REG);
}
*/

static void rtl836x_tbl_settle(void)
{
	delay(1);
}

/* A few parameters below are __xdata on purpose: the RTL836x image leaves only
 * a couple of bytes of the 128-byte direct RAM for parameter slots, so every
 * slot moved to xdata buys room the linker otherwise cannot find. */
static void rtl836x_vlan4k_set(__xdata uint16_t vid)
{
	/* No locals either -- the word expressions are built inside the calls. */
	REG_SET(RTL836x_TA_WRDATA_REG(0), (uint16_t)((rtl836x_vlan_member & RTL836x_MEMBER_MASK)
		| ((rtl836x_vlan_untag & RTL836x_UNTAG_MASK) << RTL836x_TA_VLAN0_UNTAG_SHIFT)));
	REG_SET(RTL836x_TA_WRDATA_REG(1), (uint16_t)(rtl836x_vlan_fid & RTL836x_FID_MASK));
	REG_SET(RTL836x_TA_WRDATA_REG(2), (uint16_t)
		((((rtl836x_vlan_member >> RTL836x_TA_VLAN_HI_SHIFT) & RTL836x_TA_VLAN_HI_MASK)
		  << RTL836x_TA_VLAN2_MEMBER_HI_SHIFT)
		 | (((rtl836x_vlan_untag >> RTL836x_TA_VLAN_HI_SHIFT) & RTL836x_TA_VLAN_HI_MASK)
		    << RTL836x_TA_VLAN2_UNTAG_HI_SHIFT)));
	REG_SET(RTL836x_TA_ADDR_REG, vid & RTL836x_VID_MASK);
	REG_SET(RTL836x_TA_CTRL_REG, RTL836x_TA_CTRL_CVLAN_WRITE);
	rtl836x_tbl_settle();
}

/* Fills rtl836x_vlan_member/untag/fid from the 4K (CVLAN) table. */
static void rtl836x_vlan4k_read(uint16_t vid)
{
	uint16_t w;

	REG_SET(RTL836x_TA_ADDR_REG, vid & RTL836x_VID_MASK);
	REG_SET(RTL836x_TA_CTRL_REG, RTL836x_TA_CTRL_CVLAN_READ);
	rtl836x_tbl_settle_fast();
	reg_read_m(RTL836x_TA_RDDATA_REG(0));
	w = rtl836x_sfr_u16();
	rtl836x_vlan_member = (uint16_t)(w & RTL836x_MEMBER_MASK);
	rtl836x_vlan_untag = (uint16_t)((w >> RTL836x_TA_VLAN0_UNTAG_SHIFT) & RTL836x_UNTAG_MASK);
	reg_read_m(RTL836x_TA_RDDATA_REG(1));
	w = rtl836x_sfr_u16();
	rtl836x_vlan_fid = (uint8_t)(w & RTL836x_FID_MASK);
	reg_read_m(RTL836x_TA_RDDATA_REG(2));
	w = rtl836x_sfr_u16();
	rtl836x_vlan_member |= (uint16_t)(((w >> RTL836x_TA_VLAN2_MEMBER_HI_SHIFT)
		& RTL836x_TA_VLAN_HI_MASK) << RTL836x_TA_VLAN_HI_SHIFT);
	rtl836x_vlan_untag |= (uint16_t)(((w >> RTL836x_TA_VLAN2_UNTAG_HI_SHIFT)
		& RTL836x_TA_VLAN_HI_MASK) << RTL836x_TA_VLAN_HI_SHIFT);
}

/* Fills rtl836x_pvid/member/fid from Vlan entry idx. */
static void rtl836x_vlan_cfg_get(uint8_t idx)
{
	uint16_t base = RTL836x_VLAN_MC_BASE(idx);

	reg_read_m(base);
	rtl836x_mc_member = (uint16_t)(rtl836x_sfr_u16() & RTL836x_MEMBER_MASK_WIDE);
	reg_read_m(base + 1);
	rtl836x_mc_fid = (rtl836x_sfr_u16() & RTL836x_FID_MASK);
	reg_read_m(base + 3);
	rtl836x_pvid = (uint16_t)(rtl836x_sfr_u16() & RTL836x_VID_MASK);
}

/* __reentrant keeps the four parameters on the stack instead of internal data:
 * the RTL836x image has no spare DSEG left (see the OSEG/DSEG split in the
 * linker map), and this is a management-path helper. */
static void rtl836x_vlan_cfg_set(uint8_t idx, uint16_t vid, uint16_t member, uint8_t fid) __reentrant
{
	uint16_t base = RTL836x_VLAN_MC_BASE(idx);

	REG_SET(base, (uint16_t)(member & RTL836x_MEMBER_MASK_WIDE));
	REG_SET(base + 1, (uint16_t)(fid & RTL836x_FID_MASK));
	REG_SET(base + 2, 0); //FIXME priority setting
	REG_SET(base + 3, (uint16_t)(vid & RTL836x_VID_MASK));
}

static uint8_t rtl836x_pvid_index_get(uint8_t port)
{
	reg_read_m(RTL836x_VLAN_PVID_CTRL_REG(port));
	return (uint8_t)((rtl836x_sfr_u16() >> RTL836x_VLAN_PVID_CTRL_SHIFT(port)) & RTL836x_VLAN_PVID_CTRL_MASK);
}

static void rtl836x_pvid_index_set(uint8_t port, __xdata uint8_t idx)
{
	uint16_t reg = RTL836x_VLAN_PVID_CTRL_REG(port);
	uint8_t shift = RTL836x_VLAN_PVID_CTRL_SHIFT(port);
	uint16_t v;

	reg_read_m(reg);
	v = rtl836x_sfr_u16();
	v = (uint16_t)((v & ~(((uint16_t)RTL836x_VLAN_PVID_CTRL_MASK) << shift)) |
		       (((uint16_t)(idx & RTL836x_VLAN_PVID_CTRL_MASK)) << shift));
	REG_SET(reg, v);
}

static __bit rtl836x_pvid_index_shared(uint8_t idx, uint8_t port)
{
	uint8_t p;

	for (p = 0; p < 8; p++) {	/* the fabric's UTP ports 0-7 */
		if (p != port && rtl836x_pvid_index_get(p) == idx)
			return 1;
	}
	return 0;
}

static uint8_t rtl836x_pvid_index_for_vid(uint16_t vid, __xdata uint8_t port)
{
	uint8_t i;
	uint8_t free_idx = 0xff;

	rtl836x_vlan4k_read(vid);
	if (!rtl836x_vlan_member)
		rtl836x_vlan_member = (PMASK_PORT | PMASK_CPU);

	for (i = 0; i < RTL836x_NUM_VLANS; i++) {
		rtl836x_vlan_cfg_get(i);
		if (rtl836x_mc_member && rtl836x_pvid == vid)
			return i;
		if (free_idx == 0xff && !rtl836x_mc_member && !rtl836x_pvid
		    && !rtl836x_pvid_index_shared(i, port))
			free_idx = i;
	}
	if (free_idx == 0xff)
		free_idx = (uint8_t)(vid & (RTL836x_NUM_VLANS - 1));
	rtl836x_vlan_cfg_set(free_idx, vid, rtl836x_vlan_member, 0);
	return free_idx;
}
#endif	/* SOC_FAMILY == RTL836X (VLAN core helpers) */

// Wait for the TABLE ready
static void wait_table_ready(void)
{
#if SOC_FAMILY != RTL836X
	do {
		reg_read(RTL837X_TBL_CTRL);
	} while (SFR_DATA_0 & TBL_EXECUTE);
#else
	/* SOC_FAMILY != RTL836X (table-ready wait): every caller is an RTL8372/3-only
	 * path, so RTL836x keeps 0x5cac out of the image entirely -- the RTL836x
	 * table engine has no busy bit and rtl836x_tbl_settle() is the whole
	 * protocol. */
	uint8_t i;
	for (i = 0; i < RTL836x_TBL_FAST_SETTLE_LOOPS; i++)
		reg_read(RTL836x_TA_LUT_REG);
#endif
}

void port_mirror_set(uint8_t port, __xdata uint16_t rx_pmask, __xdata uint16_t tx_pmask) __banked
{
#if SOC_FAMILY == RTL836X
	uint16_t src = (uint16_t)(rx_pmask | tx_pmask);
	uint8_t lo = 0;
	uint16_t v = 0;

	while (lo < 8 && !(src & ((uint16_t)1 << lo)))
		lo++;
	if (lo == 8)
		lo = 0;

	v = (uint16_t)(((uint16_t)(port & 0xf) << RTL836x_MIRROR_MONITOR_SHIFT)
	     | ((uint16_t)(lo & 0xf) << RTL836x_MIRROR_SOURCE_SHIFT));
	if (rx_pmask)
		v |= RTL836x_MIRROR_RX_BIT;
	if (tx_pmask)
		v |= RTL836x_MIRROR_TX_BIT;
	REG_SET(RTL836x_MIRROR_CTRL_REG, v);
	REG_SET(RTL836x_MIRROR_SRC_PMSK_REG, (uint16_t)(src & 0xff));
	return;
#else
	print_string("Mirroring port: "); print_byte(port); print_string(" with rx-mask: ");
	print_short(rx_pmask); print_string(", tx mask: "); print_short(tx_pmask);
	write_char('\n');

	REG_WRITE(RTL837x_MIRROR_CONF, rx_pmask >> 8, rx_pmask, tx_pmask >> 8, tx_pmask);
	REG_WRITE(RTL837x_MIRROR_CTRL, 0, 0, 0, (port << 1) | 0x1);
#endif
}


void port_mirror_del(void) __banked
{
#if SOC_FAMILY == RTL836X
	REG_SET(RTL836x_MIRROR_CTRL_REG, 0);
	REG_SET(RTL836x_MIRROR_SRC_PMSK_REG, 0);
#else
	REG_SET(RTL837x_MIRROR_CTRL, 0);
#endif
}


#if SOC_FAMILY == RTL836X
#define INGRESS_REG(port)	RTL836x_VLAN_ACCEPT_FRAME_TYPE_REG(port)
#define INGRESS_PORT_INVALID(port)	((((uint16_t)1 << (port)) & (PMASK_CPU | PMASK_PORT)) == 0)
#define INGRESS_SFR_INDEX(port)		(3 - (((port) & 0x7) / 4))
#define INGRESS_SFR_SHIFT(port)		(((port) & 3) << 1)
#else
/* One 32-bit register at 0x4e10, two bits per port from bit 0. */
#define INGRESS_REG(port)	RTL837x_REG_INGRESS
#define INGRESS_PORT_INVALID(port)	((port) > CPU_PORT)
#define INGRESS_SFR_INDEX(port)		(3 - ((port) / 4))
#define INGRESS_SFR_SHIFT(port)		(((port) % 4) << 1)
#endif

bool port_ingress_filter(__xdata uint8_t port, __xdata vlan_ingress_mode_t type) __banked
{
	if (INGRESS_PORT_INVALID(port) || type >= VLAN_INVALID) {
		print_string("Invalid port or ingress filter type\n");
		return false;
	}

	if (type & 0x1) {
		reg_bit_set(INGRESS_REG(port), port << 1);
	} else {
		reg_bit_clear(INGRESS_REG(port), port << 1);
	}
	if (type & 0x2) {
		reg_bit_set(INGRESS_REG(port), (port << 1) + 1);
	} else {
		reg_bit_clear(INGRESS_REG(port), (port << 1) + 1);
	}
	return true;
}

vlan_ingress_mode_t port_ingress_filter_get(__xdata uint8_t port) __banked
{
#if SOC_FAMILY == RTL836X
	if (INGRESS_PORT_INVALID(port))
		return VLAN_INVALID;

	reg_read_m(INGRESS_REG(port));
#else
	reg_read_m(RTL837x_REG_INGRESS);
	if (port > 9) {
		return VLAN_INVALID;
	}
#endif

	const uint8_t sfr_index = INGRESS_SFR_INDEX(port);
	const uint8_t shift = INGRESS_SFR_SHIFT(port);

	return (vlan_ingress_mode_t)((sfr_data[sfr_index] >> shift) & 0x03);
}


/*
 * Define a Primary VLAN ID for a port 
*/
#if SOC_FAMILY != RTL836X
static void port_pvid_write(uint8_t port, __xdata uint16_t pvid)
{
	// r4e1c:00001001 R4e1c-000017d0 r6738:00000000 R6738-00000000 (no filtering)
	uint16_t reg = RTL837x_PVID_BASE_REG + ((port >> 1) << 2);

	reg_read_m(reg);
	if (port & 0x1) {
		REG_WRITE(reg, sfr_data[0], pvid >> 4, (sfr_data[2] & 0x0f) | (pvid << 4), sfr_data[3]);
	} else {
		REG_WRITE(reg, sfr_data[0], sfr_data[1], (sfr_data[2] & 0xf0) | (pvid >> 8), pvid);
	}
}

void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked
{
	uint8_t lag = port_lag_of(port);

	if (lag == PORT_LAG_NONE) {
		port_pvid_write(port, pvid);
		return;
	}

	uint16_t members = port_lag_members_get(lag);
	for (uint8_t i = 0; i < 10; i++)
		if ((members >> i) & 1)
			port_pvid_write(i, pvid);
}

uint16_t port_pvid_get(uint8_t port) __banked
{
	uint16_t reg = RTL837x_PVID_BASE_REG + ((port >> 1) << 2);
	reg_read_m(reg);
	if (port & 0x1) {
		return (sfr_data[1] << 4) | (sfr_data[2] >> 4);
	} else {
		return ((sfr_data[2] & 0x0f) << 8) | sfr_data[3];
	}
}
#else
void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked
{
	uint8_t idx;

	print_string("\nport_pvid_set called \n");
	if (port > 7) {
		print_string("Invalid port (RTL836x fabric has ports 0-7)\n");
		return;
	}
	if (!pvid || pvid >= 0xfff) {
		print_string("Invalid PVID\n");
		return;
	}
	idx = rtl836x_pvid_index_for_vid(pvid, port);
	rtl836x_pvid_index_set(port, idx);
	print_string("port_pvid_set: " ); print_byte(port); print_string(" -> VID "); print_short(pvid);
	print_string(" (MC index "); print_byte(idx); print_string(")\n");
}

uint16_t port_pvid_get(uint8_t port) __banked
{
	if (port > 7 && port != CPU_PORT)
		return 0;
	rtl836x_vlan_cfg_get(rtl836x_pvid_index_get(port));
	return rtl836x_pvid;
}
#endif


#if SOC_FAMILY != RTL836X
void vlan_delete(uint16_t vlan) __banked
{
	if (!vlan || vlan >= 0xfff)
		return;

	vlan_name_remove(vlan);
	REG_WRITE(RTL837x_TBL_DATA_IN_A, 0, 0, 0, 0);
	REG_WRITE(RTL837X_TBL_CTRL, vlan >> 8, vlan, TBL_VLAN, TBL_WRITE | TBL_EXECUTE);
}
#else
static void rtl836x_pvid_repoint_ports(uint8_t idx)
{
	uint8_t p;

	for (p = 0; p < 8; p++) {
		if (rtl836x_pvid_index_get(p) == idx)
			rtl836x_pvid_index_set(p, rtl836x_pvid_index_for_vid(1, p));
	}
}

void vlan_delete(uint16_t vlan) __banked
{
	uint8_t i;

	if (!vlan || vlan >= 0xfff)
		return;

	print_string("\nvlan_delete called \n"); print_short(vlan);
	vlan_name_remove(vlan);
	/* Clear the 4K entry (member = 0 makes it invalid) ... */
	rtl836x_vlan_member = 0; rtl836x_vlan_untag = 0; rtl836x_vlan_fid = 0;
	rtl836x_vlan4k_set(vlan);
	for (i = 0; i < RTL836x_NUM_VLANS; i++) {
		rtl836x_vlan_cfg_get(i);
		if (rtl836x_pvid != vlan)
			continue;
		rtl836x_vlan_cfg_set(i, 0, 0, 0);
		rtl836x_pvid_repoint_ports(i);
	}
}
#endif


void vlan_name_remove(uint16_t vlan) __banked
{
	static __xdata uint16_t name_pos;
	static __xdata uint16_t entry_start;
	static __xdata uint16_t pos;
	static __xdata uint16_t entry_len;
	static __xdata uint16_t move_count;
	static __xdata uint16_t j;

	name_pos = vlan_name(vlan);
	if (name_pos == 0xffff)
		return;

	entry_start = name_pos - 3;
	pos = entry_start;

	while (pos < vlan_ptr && vlan_names[pos] != ' ')
		pos++;
	if (pos >= vlan_ptr)
		return;
	pos++;

	entry_len = pos - entry_start;
	move_count = vlan_ptr - pos;

	for (j = 0; j < move_count; j++)
		vlan_names[entry_start + j] = vlan_names[pos + j];

	vlan_ptr -= entry_len;
	vlan_names[vlan_ptr] = 0;
}


/*
 * Reads VLAN information from VLAN table
 * Returns data in sfr_data
 */
#if SOC_FAMILY != RTL836X
int8_t vlan_get(uint16_t vlan) __banked
{
	if (vlan >= 0xfff) // VLAN 4095 is special
		return -1;

	REG_WRITE(RTL837X_TBL_CTRL, vlan >> 8, vlan, TBL_VLAN, TBL_EXECUTE);

	wait_table_ready();

	reg_read_m(RTL837x_L2_DATA_OUT_A);

	return 0;
}
#else
int8_t vlan_get(uint16_t vlan) __banked
{
	if (vlan >= 0xfff) // VLAN 4095 is special
		return -1;

	rtl836x_vlan4k_read(vlan);
	if (!rtl836x_vlan_member) {
		sfr_data[0] = 0; sfr_data[1] = 0; sfr_data[2] = 0; sfr_data[3] = 0;
		return -1;
	}
	sfr_data[0] = 0x02;
	sfr_data[1] = 0x00;
	sfr_data[2] = (uint8_t)(rtl836x_vlan_member & RTL836x_MEMBER_MASK);
	sfr_data[3] = (uint8_t)(rtl836x_vlan_untag & RTL836x_UNTAG_MASK);
	return 0;
}
#endif


__xdata uint16_t vlan_name(uint16_t vlan) __banked
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
 * Create a VLAN
 * The arguments are passed in global structure vlan_settings
 * A member that is not tagged, is untagged
 */
#if SOC_FAMILY != RTL836X
void vlan_create(void) __banked
{
	if (!vlan_settings.vlan || vlan_settings.vlan >= 0xfff) {
		print_string("\nInvalid VLAN: "); print_short(vlan_settings.vlan); write_char('\n');
		return;
	}

	// For now, the CPU-port is always a tagged member:
	vlan_settings.members |= 0x0200; // Set 10th bit
	vlan_settings.tagged |= 0x0200;

	print_string("vlan: "); print_short(vlan_settings.vlan);
	print_string(", members: "); print_short(vlan_settings.members);
	print_string(", tagged: "); print_short(vlan_settings.tagged); write_char('\n');

	uint16_t a = (~vlan_settings.members) ^ vlan_settings.tagged ^ vlan_settings.members;

	// On RTL8372, port-bits 0-2 must be 0, although they are not members
	if (!machine_detected.isRTL8373) {
		a &= 0x1f8;
		vlan_settings.tagged &= 0x3f8;
	}

	// Initialize VLAN table with VLAN 1
	REG_WRITE(RTL837x_TBL_DATA_IN_A, 0x02, (a >> 6) & 0x0f, (a << 2) | (vlan_settings.members >> 8), vlan_settings.members);
	REG_WRITE(RTL837X_TBL_CTRL, vlan_settings.vlan >> 8, vlan_settings.vlan, TBL_VLAN, TBL_WRITE | TBL_EXECUTE);

	wait_table_ready();

}
#else
void vlan_create(void) __banked
{
	uint8_t i;

	if (!vlan_settings.vlan || vlan_settings.vlan >= 0xfff) {
		print_string("\nInvalid VLAN: "); print_short(vlan_settings.vlan); write_char('\n');
		return;
	}

	rtl836x_vlan_member = (uint16_t)((vlan_settings.members & RTL836x_MEMBER_MASK_WIDE)
					 | PMASK_CPU);
	rtl836x_vlan_untag = (uint16_t)((vlan_settings.members & ~vlan_settings.tagged)
					 & RTL836x_UNTAG_MASK_WIDE & ~PMASK_CPU);

	print_string("\nvlan_create called\nvlan: "); print_short(vlan_settings.vlan);
	print_string(", members: "); print_short(rtl836x_vlan_member);
	print_string(", untagged: "); print_short(rtl836x_vlan_untag); write_char('\n');

	rtl836x_vlan_fid = 0;
	rtl836x_vlan4k_set(vlan_settings.vlan);

	for (i = 0; i < RTL836x_NUM_VLANS; i++) {
		rtl836x_vlan_cfg_get(i);
		if (rtl836x_mc_member && rtl836x_pvid == vlan_settings.vlan)
			rtl836x_vlan_cfg_set(i, vlan_settings.vlan, rtl836x_vlan_member, 0);
	}

	print_string("vlan_create done \n");
}
#endif


/*
 * Configures a default VLAN 1 and enables 4k VLAN tables
 * All ports are made members of the VLAN and VLAN filtering
 * is enabled on all ports
 * PVID is set to 1 for all ports
 * Called upon reboot
 */
#if SOC_FAMILY == RTL836X
void vlan_setup(void) __banked
{
	uint8_t i;

	print_string("\nvlan_setup (RTL836x) called\n");

	// No VLAN names set up so far
	vlan_ptr = 0;
	vlan_names[0] = 0;

	rtl836x_vlan_all = (PMASK_PORT | PMASK_CPU);

	// VLAN 1: every real port + CPU, all untagged
	rtl836x_vlan_member = rtl836x_vlan_all;
	rtl836x_vlan_untag = rtl836x_vlan_all;
	rtl836x_vlan_fid = 0;
	rtl836x_vlan4k_set(1);

	// MC entry 0 mirrors VLAN 1 so each port's PVID can reference it
	rtl836x_vlan_cfg_set(0, 1, rtl836x_vlan_all, 0);

	for (i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		uint16_t misc;

		// PVID = VLAN 1 (MC index 0)
		rtl836x_pvid_index_set(i, 0);

		// Egress mode ORIGINAL: clear bits 5:4 of the port-misc register
		reg_read_m(RTL836x_PORT_MISC_CFG_REG(i));
		misc = rtl836x_sfr_u16();
		misc &= (uint16_t)~(((uint16_t)RTL836x_PORT_MISC_EGRESS_MASK) << RTL836x_PORT_MISC_EGRESS_SHIFT);
		misc |= (uint16_t)(RTL836x_PORT_MISC_EGRESS_ORIGINAL << RTL836x_PORT_MISC_EGRESS_SHIFT);
		REG_SET(RTL836x_PORT_MISC_CFG_REG(i), misc);
	}

	// 802.1Q membership lookups on
	reg_bit_set(RTL836x_VLAN_CTRL_REG, 0);

	REG_SET(RTL836x_REG_CPU_CTRL, 0x00bb);		/* CPU_EN, trap port 7 */
	REG_SET(RTL836x_REG_CPU_PORT_MASK, 0x0080);	/* port 7 in the 11-bit mask */
	REG_SET(RTL836x_CPU_EXTIF_FORCE_REG, RTL836x_CPU_FORCE_VALUE);
	REG_SET(RTL836x_CPU_PEER_FORCE_REG, RTL836x_CPU_FORCE_VALUE);
	REG_SET(RTL836x_RMA_CTRL00_REG, RTL836x_RMA_CTRL00_VALUE);

	REG_SET(RTL836x_VLAN_INGRESS_REG, RTL836x_MEMBER_MASK);

	/* Everything above is unconditional, so these only fill in what was left
	 * unset -- in practice the CPU port's PVID, which no port loop covers.
	 * They live here (not in main()) so both dies run the same bring-up
	 * sequence; main() calls this before execute_config(), so a "pvid"/
	 * "vlan"/"ingress" line from the config still wins. */
	port_pvid_default_unset();
	port_vlan_filter_default_on();
	port_vlan_egress_original();

	print_string("vlan_setup done (VLAN 1: members 0x");
	print_short(rtl836x_vlan_all); print_string(", ingress filtering off)\n");
}

#else
void vlan_setup(void) __banked
{

	// No VLAN names set up so far
	vlan_ptr = 0;
	vlan_names[0] = 0;

	// Initialize VLAN table for VLAN 1, by disabling that entry
	REG_SET(RTL837x_TBL_DATA_IN_A, machine_detected.isRTL8373? 0x0007ffff : 0x0007e3f8);

	REG_SET(RTL837X_TBL_CTRL, 0x00010303);

	wait_table_ready();

	// Set PVID 1 for every port. TODO: Skip unused ports!
	for (uint8_t i = MACHINE_MIN_PORT; i <= CPU_PORT; i++) { // CPU MAC is independent of the last user port
		uint16_t reg = RTL837x_PVID_BASE_REG + ((i >> 1) << 2);
#ifdef DEBUG
		print_byte(i); write_char(':'); write_char(' '); print_short(reg); write_char('=');
		print_short(reg);  write_char(' ');
#endif
		reg_read_m(reg);
		if (i & 0x1) {
			REG_WRITE(reg, sfr_data[0], 0, (sfr_data[2] & 0x0f) | 0x10, sfr_data[3]);
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
		// Enable INGRESS filtering for port: discard packets not belonging to member VLAN on that port
		port_ingress_vlan_filter_set(i, true);

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

	// Enable VLAN 1: user ports untagged, CPU MAC9 tagged (untag bit 19 clear)
	REG_SET(RTL837x_TBL_DATA_IN_A, machine_detected.isRTL8373? 0x0207ffff : 0x0207e3f8);	// 02: Entry valid, 7...: membership

	REG_SET(RTL837X_TBL_CTRL, 0x00010303);	// Write VLAN 1

	wait_table_ready();

#ifdef DEBUG
	print_string("\nvlan_setup, REG 0x6738: "); print_reg(0x6738);
	print_string("\nvlan_setup, REG 0x4e10: "); print_reg(0x4e10);
	print_string("\nvlan_setup, REG 0x4e18: "); print_reg(0x4e18);
	print_string("\nvlan_setup, REG 0x4e14: "); print_reg(0x4e14);
	print_string("\nvlan_setup, REG 0x4e30: "); print_reg(0x4e30);
	print_string("\nvlan_setup, REG 0x4e34: "); print_reg(0x4e34);
	print_string("\nvlan_setup, REG 0x4f48: "); print_reg(0x4f48);
	print_string("\nvlan_setup, REG 0x4f4c: "); print_reg(0x4f4c);
#endif

}
#endif	/* SOC_FAMILY == RTL836X (vlan_setup) */

#define TABLE_WAIT_TOUT	1000

#if SOC_FAMILY == RTL836X
#define RTL836x_L2_NONE			0xffff
#define RTL836x_L2_LIST_MAX		128

static __xdata uint16_t rtl836x_l2_word[RTL836x_TA_L2_NUM_WORDS];
static __xdata uint8_t rtl836x_l2_mac[6];
static __xdata uint16_t rtl836x_l2_fid;
static __xdata uint8_t rtl836x_l2_spa;
static __xdata uint16_t rtl836x_l2_portmask;
static __xdata uint8_t rtl836x_l2_static;
static __xdata uint8_t rtl836x_l2_mcast;
static __xdata uint16_t rtl836x_l2_slot;
static __xdata uint8_t rtl836x_l2_shown;
static __xdata uint16_t rtl836x_l2_e;
static __xdata uint8_t rtl836x_l2_p;
/* Status of the last management-entry write (port_l2_static_mgmt). */
static __xdata uint16_t rtl836x_l2_mgmt_status;

#define RTL836x_L2_SLOTS		0x1000

/* Read slot `addr` into rtl836x_l2_word[]. */
static void rtl836x_l2_read(uint16_t addr) __reentrant
{
	uint8_t i;

	addr &= RTL836x_TA_LUT_ADDR_MASK;
	REG_SET(RTL836x_TA_ADDR_REG, addr);
	REG_SET(RTL836x_TA_CTRL_REG, RTL836x_TA_CTRL_L2_READ_ADDR);
	rtl836x_tbl_settle_fast();
	for (i = 0; i < RTL836x_TA_L2_NUM_WORDS; i++) {
		reg_read_m(RTL836x_TA_RDDATA_REG(i));
		rtl836x_l2_word[i] = rtl836x_sfr_u16();
	}
}

static void rtl836x_l2_write_key(void) __reentrant
{
	uint8_t i;

	for (i = 0; i < RTL836x_TA_L2_NUM_WORDS; i++)
		REG_SET(RTL836x_TA_WRDATA_REG(i), rtl836x_l2_word[i]);
	REG_SET(RTL836x_TA_CTRL_REG, RTL836x_TA_CTRL_L2_WRITE);
	rtl836x_tbl_settle();
}

static uint8_t rtl836x_l2_stored(void)
{
	return (rtl836x_l2_word[RTL836x_TA_L2_W1] | rtl836x_l2_word[RTL836x_TA_L2_W2] |
		rtl836x_l2_word[RTL836x_TA_L2_W3] | rtl836x_l2_word[RTL836x_TA_L2_W4] |
		rtl836x_l2_word[RTL836x_TA_L2_W5] | rtl836x_l2_word[RTL836x_TA_L2_W6]) ? 1 : 0;
}

static uint8_t rtl836x_l2_held(void)
{
	return ((rtl836x_l2_word[RTL836x_TA_L2_W4] >> RTL836x_TA_L2_W4_AGE_SHIFT) &&
		(rtl836x_l2_word[RTL836x_TA_L2_W5] & RTL836x_TA_L2_W5_LIVE)) ? 1 : 0;
}

static void rtl836x_l2_unpack(void) __reentrant
{
	rtl836x_l2_mac[0] = (uint8_t)(rtl836x_l2_word[RTL836x_TA_L2_W2] >> 8);
	rtl836x_l2_mac[1] = (uint8_t)(rtl836x_l2_word[RTL836x_TA_L2_W2] & 0xff);
	rtl836x_l2_mac[2] = (uint8_t)(rtl836x_l2_word[RTL836x_TA_L2_W1] >> 8);
	rtl836x_l2_mac[3] = (uint8_t)(rtl836x_l2_word[RTL836x_TA_L2_W1] & 0xff);
	rtl836x_l2_mac[4] = (uint8_t)(rtl836x_l2_word[RTL836x_TA_L2_W0] >> 8);
	rtl836x_l2_mac[5] = (uint8_t)(rtl836x_l2_word[RTL836x_TA_L2_W0] & 0xff);

	rtl836x_l2_fid = (uint16_t)(rtl836x_l2_word[RTL836x_TA_L2_W3] & RTL836x_TA_L2_W3_FID_MASK);
	rtl836x_l2_static = 0;
	rtl836x_l2_mcast = (rtl836x_l2_mac[0] & 0x01) ? 1 : 0;
	rtl836x_l2_portmask = 0;
	rtl836x_l2_spa = L2_WALK_PORT_UNKNOWN;
}

static uint16_t rtl836x_l2_next(__xdata uint16_t *slot) __reentrant
{
	uint16_t s = *slot;

	while (s < RTL836x_L2_SLOTS) {
		rtl836x_l2_read(s);
		if (rtl836x_l2_held()) {
			*slot = (uint16_t)(s + 1);
			rtl836x_l2_unpack();
			return s;
		}
		s++;
	}
	*slot = RTL836x_L2_NONE;
	return RTL836x_L2_NONE;
}

static void rtl836x_l2_flush(uint16_t pmask) __reentrant
{
	uint16_t tw = TABLE_WAIT_TOUT;

	REG_SET(RTL836x_FORCE_FLUSH_REG, pmask);
	do {
		reg_read_m(RTL836x_FORCE_FLUSH_REG);
		if (--tw == 0)
			break;
	} while (rtl836x_sfr_u16() & pmask);
}

/* Walk interface for the console and the web page (rtl837x_port.h). */
__xdata struct l2_walk_entry l2_walk;

uint16_t rtl836x_l2_walk_next(__xdata uint16_t *slot) __banked __reentrant
{
	uint16_t e = rtl836x_l2_next(slot);

	if (e == RTL836x_L2_NONE)
		return L2_WALK_END;

	l2_walk.mac[0] = rtl836x_l2_mac[0];
	l2_walk.mac[1] = rtl836x_l2_mac[1];
	l2_walk.mac[2] = rtl836x_l2_mac[2];
	l2_walk.mac[3] = rtl836x_l2_mac[3];
	l2_walk.mac[4] = rtl836x_l2_mac[4];
	l2_walk.mac[5] = rtl836x_l2_mac[5];
	l2_walk.fid = rtl836x_l2_fid;
	l2_walk.port = rtl836x_l2_spa;
	l2_walk.is_static = rtl836x_l2_static;
	l2_walk.is_mcast = rtl836x_l2_mcast;
	l2_walk.portmask = rtl836x_l2_portmask;
	return e;
}

uint8_t rtl836x_l2_delete_slot(uint16_t slot) __banked __reentrant
{
	if (slot >= RTL836x_L2_SLOTS)
		return 0;

	rtl836x_l2_read(slot);
	if (!rtl836x_l2_stored())
		return 0;

	rtl836x_l2_word[RTL836x_TA_L2_W4] = 0;	/* age 0 = expired */
	rtl836x_l2_word[RTL836x_TA_L2_W5] &= (uint16_t)~RTL836x_TA_L2_W5_LIVE;
	rtl836x_l2_write_key();
	rtl836x_l2_read(slot);
	return rtl836x_l2_held() ? 0 : 1;
}

#endif	/* SOC_FAMILY == RTL836X (L2 table engine) */


/*
 * Forget the dynamic L2 entries learned on one port.
 */
void port_l2_forget_port(uint8_t port) __banked
{
#if SOC_FAMILY == RTL836X
	if (port > 7)
		return;
	rtl836x_l2_flush((uint16_t)1 << port);
#else
	REG_SET(RTL837x_L2_TBL_FLUSH_CNF, 0x0);	/* port-based, dynamic entries */
	REG_SET(RTL837x_L2_TBL_FLUSH_CTRL, L2_TBL_FLUSH_EXEC | (((uint16_t)1) << port));

	do {
		reg_read(RTL837x_L2_TBL_FLUSH_CTRL);
	} while (SFR_DATA_16);
#endif	/* SOC_FAMILY != RTL836X */
}


/*
 * Forget all dynamic L2 learned entries
 */
uint8_t port_l2_forget(void) __banked
{
#if SOC_FAMILY == RTL836X
	/* RTL836x: one hardware flush of every fabric port. */
	print_string("\nport_l2_forget (RTL836x) called\n");
	rtl836x_l2_flush(RTL836x_L2_FLUSH_ALL);
	print_string("port_l2_forget done\n");
	return 0;
#else
	// Configure the entries to be flushed:
	// port-based (bits 0-1 are 0 and dynamic entries, bit 2 specifies dynamic entries
	REG_SET(RTL837x_L2_TBL_FLUSH_CNF, 0x0);

	// Flush L2 table for all ports by setting the ports and the flush-exec bit (bit 16)
	uint16_t mask = PMASK_6;
	if (machine_detected.isRTL8373)
		mask = PMASK_9;
	REG_SET(RTL837x_L2_TBL_FLUSH_CTRL, L2_TBL_FLUSH_EXEC | mask);

	// Wait for flush completed
	do {
		reg_read_m(RTL837x_L2_TBL_FLUSH_CTRL);
	} while (sfr_data[1]);

	return 0;
#endif	/* SOC_FAMILY != RTL836X */
}


void port_l2_learned(void) __banked
{
#if SOC_FAMILY == RTL836X
	rtl836x_l2_slot = 0;
	rtl836x_l2_shown = 0;

	print_string("\n\tMAC\t\tFID\ttype\tport\tidx\n");
	while (rtl836x_l2_shown < RTL836x_L2_LIST_MAX) {
		rtl836x_l2_e = rtl836x_l2_next(&rtl836x_l2_slot);
		if (rtl836x_l2_e == RTL836x_L2_NONE)
			break;
		rtl836x_l2_shown++;
		print_byte(rtl836x_l2_mac[0]); write_char(':');
		print_byte(rtl836x_l2_mac[1]); write_char(':');
		print_byte(rtl836x_l2_mac[2]); write_char(':');
		print_byte(rtl836x_l2_mac[3]); write_char(':');
		print_byte(rtl836x_l2_mac[4]); write_char(':');
		print_byte(rtl836x_l2_mac[5]); write_char('\t');
		print_short(rtl836x_l2_fid); write_char('\t');
		print_string(rtl836x_l2_static ? "static\t" : "learned\t");
		if (rtl836x_l2_spa == L2_WALK_PORT_UNKNOWN)
			write_char('?');
		else
			print_phys_port(rtl836x_l2_spa);
		write_char('\t');
		print_short(rtl836x_l2_e);
		write_char('\n');
	}
	if (!rtl836x_l2_shown)
		print_string("(no entries -- nothing learned yet, or the table is empty)\n");
#else
	// Whait for any table action to be finished
	wait_table_ready();

	print_string("\n\tMAC\t\tVLAN\ttype\tport\n");
	__xdata uint16_t entry = 0x0000;
	__xdata uint16_t first_entry = 0xffff; // Table does not have that many entries

	while (1) {
		uint8_t port = 0;
		uint8_t lag;
		reg_read_m(RTL837x_TBL_DATA_0);
		REG_WRITE(RTL837x_TBL_DATA_0, sfr_data[0], sfr_data[1],sfr_data[2] | 0xc0, sfr_data[3]);

		REG_WRITE(RTL837X_TBL_CTRL, (entry >> 8) & 0xf, entry, TBL_L2_UNICAST, TBL_EXECUTE);

		wait_table_ready();

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
			if (sfr_data[1] & 0x1)
				print_string("\tstatic\t");
			else
				print_string("\tlearned\t");

			port |= (sfr_data[3] & 0x3) << 2;
			lag = port_lag_of(port);
			if (lag == PORT_LAG_NONE) {
				print_phys_port(port);
			} else {
				print_string("LAG");
				itoa(lag + 1);
			}
		}

		entry++;
		print_string("\n");
	}
#endif	/* SOC_FAMILY != RTL836X */
}


/*
 * Static L2 multicast entry for the link-local group 01:80:C2:00:00:<mac_last>
 * in VLAN `vid`, with member portmask `pmask` (bit 9 = CPU port).
 */

void port_l2mc_set(uint8_t mac_last, __xdata uint16_t vid, __xdata uint16_t pmask) __banked
{
#if SOC_FAMILY == RTL836X
	(void)mac_last; (void)vid; (void)pmask;
#else
	wait_table_ready();

	REG_WRITE(RTL837x_TBL_DATA_IN_A, 0xc2, 0x00, 0x00, mac_last);
	REG_WRITE(RTL837x_TBL_DATA_IN_B, 0x20 | (vid >> 8) | ((pmask & 0x3) << 6), vid, 0x01, 0x80);
	REG_WRITE(RTL837x_TBL_DATA_IN_C, 0, 0, 0, pmask >> 2);
	REG_WRITE(RTL837X_TBL_CTRL, 0, 0, TBL_L2_UNICAST, TBL_WRITE | TBL_EXECUTE);

	wait_table_ready();
#endif	/* SOC_FAMILY != RTL836X */
}


/*
 * Basic L2 configuration such as time to forget an entry
 */
void port_l2_setup(void) __banked
{
#if SOC_FAMILY == RTL836X
	print_string("\nport_l2_setup (RTL836x) called\n");

	rtl836x_l2_flush(RTL836x_L2_FLUSH_ALL);
	for (rtl836x_l2_p = 0; rtl836x_l2_p <= 7; rtl836x_l2_p++) {
		REG_SET(RTL836x_LUT_PORT_LEARN_LIMIT_REG(rtl836x_l2_p), 0x1040);
		port_isolate(rtl836x_l2_p, 0x7ff);
	}

	reg_read_m(RTL836x_PORT_SECURITY_CTRL_REG);
	REG_SET(RTL836x_PORT_SECURITY_CTRL_REG,
		(uint16_t)(rtl836x_sfr_u16() & ~RTL836x_LUT_LEARN_OVER_ACT_MASK));

	print_string("port_l2_setup (RTL836x) done\n");
#else
	port_l2_forget();

	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		// Limit the number of automatically learned MAC-Entries per port to 0x1040
		uint8_t idx = (i << 2);
		REG_SET(RTL837X_L2_LRN_PORT_CONSTRAINT + idx, 0x00001040);

		// All ports may communicate with each other and CPU-Port
		uint16_t mask = PMASK_CPU | PMASK_6;
		if (machine_detected.isRTL8373)
			mask = PMASK_CPU | PMASK_9;
		REG_SET(RTL837X_PORT_ISOLATION_BASE + idx, mask);
	}
	// When maximim entries learned, then simply flood the packet
	reg_bit_set(RTL837X_L2_LRN_PORT_CONSTRT_ACT, 0);

#endif	/* SOC_FAMILY != RTL836X */
}


void port_media_show(uint8_t port) __banked
{
	uint8_t pause;

	if (!machine.is_sfp[port]) {
		reg_read_m(RTL837X_REG_LINKS_STS);
		if (sfr_data[(port / 8) + 1] >> (port % 8) & 1) {
			print_string("Link role: ");
			reg_read_m(RTL837X_MAC_MSTR_SLV_STS);
			if (sfr_data[3 - (port >> 3)] & (1 << (port & 7)))
				print_string("master\n");
			else
				print_string("slave\n");
		}
	}

	print_string("Pause negotiated:");
	reg_read_m(RTL837X_MAC_TX_PAUSE_STS);
	pause = sfr_data[3 - (port >> 3)] & (1 << (port & 7));
	if (pause)
		print_string(" TX");
	reg_read_m(RTL837X_MAC_RX_PAUSE_STS);
	if (sfr_data[3 - (port >> 3)] & (1 << (port & 7))) {
		print_string(" RX");
		pause = 1;
	}
	if (!pause)
		print_string(" none");
	write_char('\n');
}

#define MIB_WAIT_TOUT	256

#if SOC_FAMILY == RTL836X
__xdata uint8_t mib_value[8];	/* big-endian 64-bit counter value */

static __xdata uint8_t mib_busy_stuck;

void rtl836x_mib_reset(void) __banked
{
	mib_busy_stuck = 0;
}

static __code const uint8_t mib_length[RTL836x_MIBS_NUMBER] = {
	4, 2, 2, 2, 2, 2, 2, 2, 2,		/* 0-8   */
	4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 9-19  */
	4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 20-30 */
	2, 2, 2, 2, 2, 2			/* 31-36 */
};

static __code const uint16_t mib_offset[RTL836x_MIBS_NUMBER] = {
	0, 4, 6, 8, 10, 12, 14, 16, 18,		/* 0-8   */
	40, 44, 46, 48, 50, 52, 54, 56, 58,	/* 9-17  */
	20, 22,					/* 18-19 */
	60, 64, 66, 68, 70, 72, 74, 76,		/* 20-27 */
	78, 80, 82, 84, 86, 88, 90,		/* 28-34 */
	0xffff, 0xffff				/* 35-36 */
};

static void mib_value_clear(void) __reentrant
{
	uint8_t i;
	for (i = 0; i < 8; i++)
		mib_value[i] = 0;
}

static void mib_put_u32_hi(uint32_t v) __reentrant
{
	mib_value[0] = (uint8_t)(v >> 24);
	mib_value[1] = (uint8_t)(v >> 16);
	mib_value[2] = (uint8_t)(v >> 8);
	mib_value[3] = (uint8_t)v;
}

static void mib_put_u32_lo(uint32_t v) __reentrant
{
	mib_value[4] = (uint8_t)(v >> 24);
	mib_value[5] = (uint8_t)(v >> 16);
	mib_value[6] = (uint8_t)(v >> 8);
	mib_value[7] = (uint8_t)v;
}

uint32_t rtl836x_mib_result(void) __banked __reentrant
{
	return ((uint32_t)mib_value[4] << 24) | ((uint32_t)mib_value[5] << 16)
	     | ((uint32_t)mib_value[6] << 8) | mib_value[7];
}

uint8_t rtl836x_mib_read(uint8_t idx, uint8_t port) __banked __reentrant
{
	uint16_t mib_off, widx;
	uint8_t len;
	uint16_t tw;
	uint32_t v;

	mib_value_clear();
	if (mib_busy_stuck || idx >= RTL836x_MIBS_NUMBER || port > MACHINE_MAX_PORT)
		return 1;

	if (idx == RTL836x_MIB_LEARN_DISCARD) {
		mib_off = RTL836x_MIB_LEARNENTRYDISCARD_OFFSET;
		len = 2;
	} else {
		mib_off = mib_offset[idx];
		if (mib_off == 0xffff)
			return 1;
		mib_off += (uint16_t)RTL836x_MIB_PORT_OFFSET * port;
		len = mib_length[idx];
	}

	REG_SET(RTL836x_MIB_ADDRESS_REG, (uint16_t)(mib_off >> 2));

	/* Bounded BUSY wait (see above). */
	tw = MIB_WAIT_TOUT;
	do {
		reg_read_m(RTL836x_MIB_CTRL_REG);
		if (--tw == 0) {
			mib_busy_stuck = 1;
			return 1;
		}
	} while (SFR_DATA_0 & RTL836x_MIB_BUSY_FLAG);

	if (SFR_DATA_0 & RTL836x_MIB_RESET_FLAG)
		return 1;

	widx = (uint16_t)(mib_off & 2);
	reg_read_m((uint16_t)(RTL836x_MIB_COUNTER_BASE_REG + widx));
	v = rtl836x_sfr_u16();
	if (len == 4) {
		reg_read_m((uint16_t)(RTL836x_MIB_COUNTER_BASE_REG + widx + 1));
		v |= (uint32_t)rtl836x_sfr_u16() << 16;
	}
	mib_put_u32_lo(v);
	return 0;
}

/* Low 32 bits of one counter, as a number (0 on error). */
static uint32_t mib_read_u32(uint8_t idx, uint8_t port) __reentrant
{
	uint32_t v;

	if (rtl836x_mib_read(idx, port))
		return 0;
	v = ((uint32_t)mib_value[4] << 24) | ((uint32_t)mib_value[5] << 16)
	  | ((uint32_t)mib_value[6] << 8) | mib_value[7];
	return v;
}

uint8_t rtl836x_mib_slot(uint8_t slot, uint8_t port) __banked __reentrant
{
	uint8_t hi = 0xff, lo = 0xff;

	switch (slot) {
	case 0:  return rtl836x_mib_read(RTL836x_MIB_IF_IN_OCTETS, port);
	case 1:  return rtl836x_mib_read(RTL836x_MIB_IF_OUT_OCTETS, port);
	case 2:  return rtl836x_mib_read(RTL836x_MIB_IF_IN_UCAST, port);
	case 3:  return rtl836x_mib_read(RTL836x_MIB_ETHER_MCAST, port);
	case 4:  return rtl836x_mib_read(RTL836x_MIB_ETHER_BCAST, port);
	case 5:  return rtl836x_mib_read(RTL836x_MIB_IF_OUT_UCAST, port);
	case 6:  return rtl836x_mib_read(RTL836x_MIB_IF_OUT_MCAST, port);
	case 7:  return rtl836x_mib_read(RTL836x_MIB_IF_OUT_BCAST, port);
	case 8:  hi = RTL836x_MIB_IF_OUT_DISCARDS; lo = RTL836x_MIB_DOT1D_PORT_IN_DISCARDS; break;
	case 9:  hi = RTL836x_MIB_DOT3_SINGLE_COLL; lo = RTL836x_MIB_DOT3_MULTI_COLL; break;
	case 10: hi = RTL836x_MIB_DOT3_DEFERRED; lo = RTL836x_MIB_DOT3_LATE_COLL; break;
	case 11: hi = RTL836x_MIB_DOT3_EXCESSIVE_COLL; lo = RTL836x_MIB_DOT3_SYMBOL_ERR; break;
	case 12: hi = RTL836x_MIB_DOT3_CTRL_UNKNOWN; lo = RTL836x_MIB_DOT3_IN_PAUSE; break;
	case 13: hi = RTL836x_MIB_DOT3_OUT_PAUSE; lo = RTL836x_MIB_ETHER_DROP_EVENTS; break;
	case 14: hi = RTL836x_MIB_IF_OUT_BCAST; lo = RTL836x_MIB_IF_OUT_MCAST; break;
	case 15: hi = RTL836x_MIB_DOT3_FCS_ERR; lo = RTL836x_MIB_DOT3_FCS_ERR; break;
	case 16: hi = RTL836x_MIB_ETHER_UNDERSIZE; lo = RTL836x_MIB_ETHER_UNDERSIZE; break;
	case 17: hi = RTL836x_MIB_ETHER_OVERSIZE; lo = RTL836x_MIB_ETHER_OVERSIZE; break;
	case 18: hi = RTL836x_MIB_ETHER_FRAGMENTS; lo = RTL836x_MIB_ETHER_FRAGMENTS; break;
	case 19: hi = RTL836x_MIB_ETHER_JABBERS; lo = RTL836x_MIB_ETHER_JABBERS; break;
	case 20: hi = RTL836x_MIB_ETHER_COLLISIONS; lo = RTL836x_MIB_ETHER_PKTS_64; break;
	case 21: hi = RTL836x_MIB_ETHER_PKTS_64; lo = RTL836x_MIB_ETHER_PKTS_65_127; break;
	default: mib_value_clear(); return 0;
	}

	mib_value_clear();
	if (lo != 0xff)
		mib_put_u32_lo(mib_read_u32(lo, port));
	if (hi != 0xff)
		mib_put_u32_hi(mib_read_u32(hi, port));
	return 0;
}

uint8_t rtl836x_mib_status(uint8_t which, uint8_t port) __banked __reentrant
{
	uint32_t v = 0;

	switch (which) {
	case MIB_ST_TX_GOOD:
		v = mib_read_u32(RTL836x_MIB_IF_OUT_UCAST, port)
		  + mib_read_u32(RTL836x_MIB_IF_OUT_MCAST, port)
		  + mib_read_u32(RTL836x_MIB_IF_OUT_BCAST, port);
		break;
	case MIB_ST_RX_GOOD:
		v = mib_read_u32(RTL836x_MIB_IF_IN_UCAST, port)
		  + mib_read_u32(RTL836x_MIB_ETHER_MCAST, port)
		  + mib_read_u32(RTL836x_MIB_ETHER_BCAST, port);
		break;
	case MIB_ST_TX_BAD:
		v = mib_read_u32(RTL836x_MIB_DOT3_SINGLE_COLL, port)
		  + mib_read_u32(RTL836x_MIB_DOT3_MULTI_COLL, port)
		  + mib_read_u32(RTL836x_MIB_DOT3_DEFERRED, port)
		  + mib_read_u32(RTL836x_MIB_DOT3_LATE_COLL, port)
		  + mib_read_u32(RTL836x_MIB_DOT3_EXCESSIVE_COLL, port)
		  + mib_read_u32(RTL836x_MIB_DOT3_OUT_PAUSE, port)
		  + mib_read_u32(RTL836x_MIB_IF_OUT_DISCARDS, port);
		break;
	default: /* MIB_ST_RX_BAD */
		v = mib_read_u32(RTL836x_MIB_DOT3_FCS_ERR, port)
		  + mib_read_u32(RTL836x_MIB_DOT3_SYMBOL_ERR, port)
		  + mib_read_u32(RTL836x_MIB_ETHER_UNDERSIZE, port)
		  + mib_read_u32(RTL836x_MIB_ETHER_OVERSIZE, port)
		  + mib_read_u32(RTL836x_MIB_ETHER_FRAGMENTS, port)
		  + mib_read_u32(RTL836x_MIB_ETHER_JABBERS, port);
		break;
	}
	mib_value_clear();
	mib_put_u32_lo(v);
	return 0;
}
#endif	/* SOC_FAMILY == RTL836X (MIB counters) */


void port_stats_print(void) __banked
{
#if SOC_FAMILY == RTL836X
	rtl836x_mib_reset();
#endif
	print_string("\nPort\tState\tLink\tTxGood\t\tTxBad\t\tRxGood\t\tRxBad\t\tMACs\n");
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		print_phys_port(i); write_char('\t');

#if SOC_FAMILY == RTL836X
		{
			uint8_t b = 99;

			reg_read_m(RTL836x_PORT_STATUS_REG(i));
			if (SFR_DATA_0 & RTL836x_PORT_STATUS_LINK) {
				print_string("On\t");
				b = (uint8_t)(SFR_DATA_0 & RTL836x_PORT_STATUS_SPEED_MASK);
			} else {
				print_string("Off\t");
			}
			switch (b) {
			case 0: print_string("10M\t"); break;
			case 1: print_string("100M\t"); break;
			case 2: print_string("1000M\t"); break;
			default: print_string("Down\t"); break;
			}
			rtl836x_mib_status(MIB_ST_TX_GOOD, i);
			print_long(rtl836x_mib_result()); write_char('\t');
			rtl836x_mib_status(MIB_ST_TX_BAD, i);
			print_long(rtl836x_mib_result()); write_char('\t');
			rtl836x_mib_status(MIB_ST_RX_GOOD, i);
			print_long(rtl836x_mib_result()); write_char('\t');
			rtl836x_mib_status(MIB_ST_RX_BAD, i);
			print_long(rtl836x_mib_result()); write_char('\t');
			reg_read_m(RTL836x_LUT_PORT_LEARN_CNT_REG(i));
			itoa_short(SFR_DATA_U16 & RTL836x_LUT_PORT_LEARN_CNT_MASK);
			print_string("\n");
		}
#else
		if (!machine.is_sfp[i]) {
			phy_read(i, PHY_MMD31, 0xa610);
			if (SFR_DATA_8 == 0x20)
				print_string("On\t");
			else
				print_string("Off\t");
		} else {  // An SFP Module
			if (!gpio_pin_test(machine.sfp_port[machine.is_sfp[i]-1].pin_detect)) {
				print_string("SFP IN\t");
			} else {
				print_string("NO SFP\t");
			}
		}

		uint8_t b = 0;

		// Determine link state
		reg_read_m(RTL837X_REG_LINKS_STS);
		if(!((sfr_data[(i / 8) + 1] >> ( i % 8  ) & 1)))
		{
			b = 99;
		}
		else
		{
			if (i < 8)
				reg_read_m(RTL837X_REG_LINKS);
			else
				reg_read_m(RTL837X_REG_LINKS_89);

			b = sfr_data[3 - ((i & 7) >> 1)];	
			b = (i & 1) ? b >> 4 : b & 0xf;
		}

		switch (b) {
		case 0:
			print_string("10M\t");
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
		case 6:
			print_string("5G\t");
			break;
		case 99:
			print_string("Down\t");
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

		reg_read(RTL837X_L2_LRN_PORT_CONSTRT_CNT + (i << 2));
		itoa_short(SFR_DATA_U16 & L2_LRN_PORT_CNT_MASK);
		print_string("\n");
#endif	/* SOC_FAMILY != RTL836X */
	}
}


#if SOC_FAMILY == RTL836X
#define ISOLATION_REG(port)	RTL836x_PORT_ISOLATION_REG(port)
#else
#define ISOLATION_REG(port)	(RTL837X_PORT_ISOLATION_BASE + ((port) << 2))
#endif

void port_isolate(uint8_t port, __xdata uint16_t pmask) __banked
{
	if (port <= MACHINE_MAX_PORT)
		REG_SET(ISOLATION_REG(port), pmask);
}


uint16_t port_isolation_get(uint8_t port) __banked
{
	if (port > MACHINE_MAX_PORT)
		return 0;

	reg_read_m(ISOLATION_REG(port));
	return ((uint16_t)sfr_data[2]) << 8 | sfr_data[3];
}


#if SOC_FAMILY == RTL836X
static void rtl836x_phy_reg_write(uint8_t port, uint8_t reg, uint16_t v)
{
	REG_SET((uint16_t)(RTL836x_PHY_BASE + ((uint16_t)port << 5) + reg), v);
}

static void rtl836x_eee_mac_set(uint8_t port, uint8_t enable)
{
	reg_read_m(RTL836x_PORT_EEE_CFG_REG(port));
	if (enable) {
		/* EEE on: TX/RX enable + both speed bits (100M + Giga). */
		sfr_data[2] |= (uint8_t)((RTL836x_EEE_TX_BIT | RTL836x_EEE_RX_BIT |
			RTL836x_EEE_GIGA_BIT | RTL836x_EEE_100M_BIT) >> 8);
		sfr_data[3] |= (uint8_t)(RTL836x_EEE_TX_BIT | RTL836x_EEE_RX_BIT |
			RTL836x_EEE_GIGA_BIT | RTL836x_EEE_100M_BIT);
	} else {
		sfr_data[2] &= (uint8_t)~((RTL836x_EEE_TX_BIT | RTL836x_EEE_RX_BIT |
			RTL836x_EEE_GIGA_BIT | RTL836x_EEE_100M_BIT) >> 8);
		sfr_data[3] &= (uint8_t)~(RTL836x_EEE_TX_BIT | RTL836x_EEE_RX_BIT |
			RTL836x_EEE_GIGA_BIT | RTL836x_EEE_100M_BIT);
	}
	reg_write_m(RTL836x_PORT_EEE_CFG_REG(port));
}

static void rtl836x_eee_phy_set(uint8_t port, uint8_t enable)
{
	rtl836x_phy_reg_write(port, 31, RTL836x_EEE_PHY_REG31_PAGE5);
	rtl836x_phy_reg_write(port, 5, RTL836x_EEE_PHY_REG5);
	rtl836x_phy_reg_write(port, 6, enable ? RTL836x_EEE_PHY_REG6_ENABLE
					       : RTL836x_EEE_PHY_REG6_DISABLE);
	rtl836x_phy_reg_write(port, 31, RTL836x_EEE_PHY_REG31_PAGE7);
	rtl836x_phy_reg_write(port, RTL836x_EEE_PHY_REG30, RTL836x_EEE_PHY_REG30_VALUE);
	rtl836x_phy_reg_write(port, RTL836x_EEE_PHY_REG21,
		enable ? RTL836x_EEE_PHY_ENABLE_REG21 : RTL836x_EEE_PHY_DISABLE_REG21);
	rtl836x_phy_reg_write(port, 31, RTL836x_EEE_PHY_REG31_PAGE0);
}
#endif	/* SOC_FAMILY == RTL836X (EEE) */

void port_eee_enable(__xdata uint8_t port,__xdata uint8_t speed) __banked
{
#if SOC_FAMILY == RTL836X
	(void)speed;
	rtl836x_eee_mac_set(port, 1);
	rtl836x_eee_phy_set(port, 1);
	print_string("EEE on for "); print_phys_port(port); write_char('\n');
#else
	if (machine.is_sfp[port])
	{
		print_string("EEE can't be enabled for SFP port "); print_phys_port(port); print_string("\n");
		return;
	}

	REG_SET(RTL837X_EEE_CTRL_BASE + (port << 8), EEE_RX_ENABLE | EEE_TX_ENABLE);
	print_string("EEE on for "); print_phys_port(port); print_string(" speed "); 
	// Enable all speeds up to the specified speed
	if (speed & EEE_100) {
			print_string("100m\n");
			// Enable EEE advertisement for 100BASE-T via EEE Advertisement Reg
			phy_write(port, PHY_MMD_AN, PHY_EEE_ADV, PHY_EEE_BIT_100M);
			if (!(speed & EEE_NORESET))
				phy_reset(port);
			return;
	}
	if (speed & EEE_1000) {
			print_string("1g\n");
			// Disable EEE advertisement for 2.5GBASE-T via EEE Advertisement Reg 2
			phy_write(port, PHY_MMD_AN, PHY_EEE_ADV2, 0);
			// Enable EEE advertisement for 100/1000BASE-T via EEE Advertisement Reg
			phy_write(port, PHY_MMD_AN, PHY_EEE_ADV, PHY_EEE_BIT_1G | PHY_EEE_BIT_100M);
			if (!(speed & EEE_NORESET))
				phy_reset(port);
			return;
	}
	if (speed & EEE_2G5) {
			print_string("2g5\n");
			// Enable EEE advertisement for 100/1000BASE-T via EEE Advertisement Reg
			phy_write(port, PHY_MMD_AN, PHY_EEE_ADV, PHY_EEE_BIT_1G | PHY_EEE_BIT_100M);
			// Enable EEE advertisement for 2.5GBASE-T via EEE Advertisement Reg 2
			phy_write(port, PHY_MMD_AN, PHY_EEE_ADV2, PHY_EEE_BIT_2G5);
			if (!(speed & EEE_NORESET))
				phy_reset(port);
			return;
	}
	if (speed & EEE_5G) {
			print_string("5g\n");
			// Enable EEE advertisement for 100/1000BASE-T via EEE Advertisement Reg
			phy_write(port, PHY_MMD_AN, PHY_EEE_ADV, PHY_EEE_BIT_1G | PHY_EEE_BIT_100M);
			// Enable EEE advertisement for 2.5GBASE-T via EEE Advertisement Reg 2
			phy_write(port, PHY_MMD_AN, PHY_EEE_ADV2, PHY_EEE_BIT_2G5 | PHY_EEE_BIT_5G);
			if (!(speed & EEE_NORESET))
				phy_reset(port);
			return;
	}
	if (speed & EEE_10G) {
			print_string("10g\n");
			// Enable EEE advertisement for 100/1000BASE-T via EEE Advertisement Reg
			phy_write(port, PHY_MMD_AN, PHY_EEE_ADV, PHY_EEE_BIT_10G | PHY_EEE_BIT_1G | PHY_EEE_BIT_100M);
			// Enable EEE advertisement for 2.5GBASE-T via EEE Advertisement Reg 2
			phy_write(port, PHY_MMD_AN, PHY_EEE_ADV2, PHY_EEE_BIT_2G5 | PHY_EEE_BIT_5G);
			if (!(speed & EEE_NORESET))
				phy_reset(port);
			return;
	}
#endif
}


void port_eee_disable(uint8_t port) __banked
{
#if SOC_FAMILY == RTL836X
	rtl836x_eee_mac_set(port, 0);
	rtl836x_eee_phy_set(port, 0);
	print_string("EEE off for "); print_phys_port(port); write_char('\n');
#else
	if (machine.is_sfp[port])
		return;

	print_string("EEE off for "); print_phys_port(port); write_char('\n');
	REG_SET(RTL837X_EEE_CTRL_BASE + (port << 8), 0);
	// Disable EEE advertisement for 100/1000BASE-T via EEE Advertisement Reg
	phy_write(port, PHY_MMD_AN, PHY_EEE_ADV, 0);
	// Disable EEE advertisement for 2.5GBASE-T via EEE Advertisement Reg 2
	phy_write(port, PHY_MMD_AN, PHY_EEE_ADV2, 0);
	phy_reset(port);
#endif
}


void port_eee_status(uint8_t port) __banked
{
#if SOC_FAMILY == RTL836X
	uint16_t v;

	reg_read_m(RTL836x_PORT_EEE_CFG_REG(port));
	v = rtl836x_sfr_u16();
	print_string("Port: "); print_phys_port(port);
	print_string(": ");
	if (v & RTL836x_EEE_100M_BIT)
		print_string(" 100M");
	else
		print_string("     ");
	if (v & RTL836x_EEE_GIGA_BIT)
		print_string(" 1G");
	else
		print_string("   ");
	if (v & RTL836x_EEE_RX_BIT)
		print_string(" rx");
	else
		print_string("   ");
	if (v & RTL836x_EEE_TX_BIT)
		print_string(" tx");
	else
		print_string("   ");
	if (v & RTL836x_EEE_LPI_BIT)
		print_string("   LPI active");
	else
		print_string("   LPI idle");
	write_char('\n');
#else
	print_string("Port: "); print_phys_port(port);
	print_string(": ");
	if (machine.is_sfp[port]) {
		print_string("SFP\n");
		return;
	}

	uint16_t v;
	print_string("Advertising: ");

	if (machine.n_10g) {
		phy_read(port, PHY_MMD_AN, PHY_EEE_ADV);
		v = SFR_DATA_U16;
		if (v & PHY_EEE_BIT_10G)
			print_string(" 10G");
		else
			print_string("    ");
	}
	phy_read(port, PHY_MMD_AN, PHY_EEE_ADV2);
	v = SFR_DATA_U16;
	if (machine.n_10g) {
		if (v & PHY_EEE_BIT_5G)
			print_string(" 5G");
		else
			print_string("   ");
	}
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
	if (machine.n_10g) {
		phy_read(port, PHY_MMD_AN, PHY_EEE_LP_ABILITY);
		v = SFR_DATA_U16;
		if (v & PHY_EEE_BIT_10G)
			print_string(" 10G");
		else
			print_string("    ");
	}
	phy_read(port, PHY_MMD_AN, PHY_EEE_LP_ABILITY2);
	v = SFR_DATA_U16;
	if (machine.n_10g) {
		if (v & PHY_EEE_BIT_5G)
			print_string(" 5G");
		else
			print_string("   ");
	}
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
#endif
}


void port_eee_enable_all(__xdata uint8_t speed) __banked
{
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		if (i == 3 && machine.n_10g) {
			port_eee_enable(i, speed);
		} else if (i == 8 && machine.n_10g == 2) {
			port_eee_enable(i, speed);
		} else {
			if (speed & EEE_10G)
				port_eee_enable(i, (speed & EEE_NORESET) | EEE_2G5);
			else
				port_eee_enable(i, speed);
		}
	}
}


void port_eee_disable_all(void) __banked
{
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		port_eee_disable(i);
	}
}


void port_eee_status_all(void) __banked
{
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
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
 * Reads the member port bitmask of a Link Aggregation Group.
 * The groups have numbers 0-3; bit n is set when logical port n is a member.
 * The bitmask reflects what the hardware holds, so it covers groups set up
 * statically and groups a protocol brought up, without either having to say so.
 */
uint16_t port_lag_members_get(uint8_t lag) __banked
{
#if SOC_FAMILY == RTL836X
	if (lag > 3)
		return 0;
	reg_read_m(RTL836x_TRUNK_GROUP_REG);
	return (uint16_t)((rtl836x_sfr_u16() >> RTL836x_TRUNK_GROUP_SHIFT(lag))
			  & RTL836x_TRUNK_GROUP_MASK);
#else
	reg_read(RTL837X_TRK_MBR_CTRL_BASE + (lag << 2));
	return ((uint16_t)SFR_DATA_8 << 8) | SFR_DATA_0;
#endif
}

uint8_t port_lag_of(uint8_t port) __banked
{
	for (uint8_t lag = 0; lag < 4; lag++)
		if ((port_lag_members_get(lag) >> port) & 1)
			return lag;
	return PORT_LAG_NONE;
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
#if SOC_FAMILY == RTL836X
	uint16_t v;

	if (lag > 3) {
		print_string("Link aggregation group out of range\n");
		return;
	}
	reg_read_m(RTL836x_TRUNK_GROUP_REG);
	v = rtl836x_sfr_u16();
	v = (uint16_t)(v & ~(RTL836x_TRUNK_GROUP_MASK << RTL836x_TRUNK_GROUP_SHIFT(lag)));
	v |= (uint16_t)((members & RTL836x_TRUNK_GROUP_MASK) << RTL836x_TRUNK_GROUP_SHIFT(lag));
	REG_SET(RTL836x_TRUNK_GROUP_REG, v);
	print_string("port_lag_members_set, lag: "); print_byte(lag);
	print_string(", members: "); print_short(members); write_char('\n');
	return;
#else
	print_string("port_lag_members_set, lag: "); print_byte(lag); print_string(", members: "); print_short(members);
	write_char('\n');
	if (lag > 3) {
		print_string("Link aggregation group out of range\n");
		return;
	}
	reg_read_m(RTL837X_TRK_HASH_CTRL_BASE + (lag << 2));
	if (!(sfr_data[0] | sfr_data[1] | sfr_data[2])
	    && (sfr_data[3] == LAG_HASH_RESET || sfr_data[3] == 0))
		REG_SET(RTL837X_TRK_HASH_CTRL_BASE + (lag << 2), LAG_HASH_DEFAULT);
	REG_WRITE(RTL837X_TRK_MBR_CTRL_BASE + (lag << 2), 0, 0, members >> 8, members & 0xff);
#endif
}


/*
 * Configures the hash algorithm used for a LAG
 * lag is the Group to configure and hash is a bitmask
 */
void port_lag_hash_set(__xdata uint8_t lag, __xdata uint8_t hash_bits) __banked
{
#if SOC_FAMILY == RTL836X
	if (lag > 3) {
		print_string("Link aggregation group out of range\n");
		return;
	}
	reg_read_m(RTL836x_TRUNK_CTRL_REG);
	REG_SET(RTL836x_TRUNK_CTRL_REG,
		(uint16_t)((rtl836x_sfr_u16() & 0xff00) | (hash_bits & 0x7f)));
	print_string("port_lag_hash_set, lag: "); print_byte(lag);
	print_string(", hash: "); print_byte(hash_bits); write_char('\n');
	return;
#else
	print_string("port_lag_hash_set, lag: "); print_byte(lag); print_string(", hash: "); print_byte(hash_bits);
	write_char('\n');
	if (lag > 3) {
		print_string("Link aggregation group out of range\n");
		return;
	}
	REG_WRITE(RTL837X_TRK_HASH_CTRL_BASE + (lag << 2), 0, 0, 0, hash_bits);
#endif
}

void print_port_ingress_filter_mode(vlan_ingress_mode_t mode) __banked
{
	switch (mode) {
	case VLAN_UNTAGGED:
		print_string("Untag.");
		break;
	case VLAN_TAGGED:
		print_string("Tagged");
		break;
	case VLAN_ALL:
		print_string("Any");
		break;
	default:
		print_string("!!err!!");
	}
}

void print_vlan_ingress_port(uint8_t log_port) __banked
{
	print_phys_port(log_port);write_char('\t');
	print_short(port_pvid_get(log_port));write_char('\t');
	print_port_ingress_filter_mode(port_ingress_filter_get(log_port));write_char('\t');
	port_ingress_vlan_filter_get(log_port) ? print_string("Enabled") : print_string("Disabled");
	write_char('\n');
}
/*
 * Dumps the VLAN ingress configuration
 */
void vlan_dump(void) __banked
{
	print_string("Ingress VLAN configuration:\n");
	print_string("Port\tPVID\tType\tFiltering\n");
	for (uint8_t port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++) {
		print_vlan_ingress_port(port);
	}
#if SOC_FAMILY != RTL836X
	print_vlan_ingress_port(9);
#else
	print_vlan_ingress_port(CPU_PORT);
#endif

	write_char('\n');
	print_string("Type - Which frame types are allowed: untagged, tagged or any\n");
	print_string("Filtering - Whether packets not belonging to member VLANs on that port are dropped\n");
	print_string("PVID - Assumed VLAN for untagged packets\n");

#if SOC_FAMILY == RTL836X
	print_string("\nCVLAN (4K) readback via the 0x0500 engine:\n");
	for (uint8_t t = 0; t < 2; t++) {
		uint16_t v = t ? 0x064 : 1;
		rtl836x_vlan4k_read(v);
		print_string("  VID "); print_short(v);
		print_string(": member="); print_short(rtl836x_vlan_member);
		print_string(" untag="); print_short(rtl836x_vlan_untag);
		print_string(" fid=0x"); print_byte(rtl836x_vlan_fid);
		write_char('\n');
	}
	print_string("  MC idx of port 0 PVID: ");
	print_byte(rtl836x_pvid_index_get(0));
	print_string(", its VID: "); print_short(port_pvid_get(0));
	write_char('\n');
#endif
}


/** Set the ingress VLAN filtering */
#if SOC_FAMILY != RTL836X
bool port_ingress_vlan_filter_set(uint8_t port, __xdata bool enabled) __banked
{
	if (!MACHINE_PORT_VALID(port) && port != CPU_PORT) {
		return false;
	}
	if (enabled)
		reg_bit_set(RTL837X_VLAN_PORT_IGR_FLTR, port);
	else
		reg_bit_clear(RTL837X_VLAN_PORT_IGR_FLTR, port);

	return true;
}

/** Get the ingress VLAN filtering status */
bool port_ingress_vlan_filter_get(uint8_t port) __banked
{
	if (!MACHINE_PORT_VALID(port) && port != CPU_PORT) {
		return false;
	}

	return reg_bit_test(RTL837X_VLAN_PORT_IGR_FLTR, port);
}
#else
/* True when port's PVID index names an empty MC entry (member 0, VID 0). */
static __bit rtl836x_pvid_is_unset(uint8_t port)
{
	rtl836x_vlan_cfg_get(rtl836x_pvid_index_get(port));
	return (!rtl836x_mc_member && !rtl836x_pvid) ? 1 : 0;
}

static void rtl836x_mc_member_add_cpu(uint8_t idx)
{
	uint16_t base = RTL836x_VLAN_MC_BASE(idx);

	reg_read_m(base);
	REG_SET(base, (uint16_t)(rtl836x_sfr_u16() | PMASK_CPU));
}

void port_pvid_default_unset(void) __banked
{
	uint8_t p;
	__bit needed = 0;

	rtl836x_vlan4k_read(1);
	if (!rtl836x_vlan_member) {
		rtl836x_vlan_member = (PMASK_PORT | PMASK_CPU);
		rtl836x_vlan_untag = (PMASK_PORT | PMASK_CPU);
		rtl836x_vlan_fid = 0;
		rtl836x_vlan4k_set(1);
	} else if (!(rtl836x_vlan_member & PMASK_CPU)) {
		rtl836x_vlan_member |= PMASK_CPU;
		rtl836x_vlan_untag |= PMASK_CPU;
		rtl836x_vlan4k_set(1);
	}

	for (p = 0; p < 8; p++)
		if (rtl836x_pvid_is_unset(p))
			needed = 1;
	if (rtl836x_pvid_is_unset(CPU_PORT))
		needed = 1;
	if (!needed)
		return;

	for (p = 0; p < 8; p++) {
		if (rtl836x_pvid_is_unset(p))
			rtl836x_pvid_index_set(p, rtl836x_pvid_index_for_vid(1, p));
	}

	if (rtl836x_pvid_is_unset(CPU_PORT)) {
		uint8_t cpu_idx = rtl836x_pvid_index_for_vid(1, CPU_PORT);

		rtl836x_pvid_index_set(CPU_PORT, cpu_idx);
		rtl836x_mc_member_add_cpu(cpu_idx);
	}
}

void port_vlan_filter_default_on(void) __banked
{
	REG_SET(RTL836x_VLAN_INGRESS_REG, (uint16_t)RTL836x_MEMBER_MASK);
	reg_bit_set(RTL836x_VLAN_CTRL_REG, 0);
}

void port_vlan_egress_original(void) __banked
{
	uint8_t i;

	for (i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		reg_read_m(RTL836x_PORT_MISC_CFG_REG(i));
		sfr_mask_data(0, (uint8_t)(RTL836x_PORT_MISC_EGRESS_MASK << RTL836x_PORT_MISC_EGRESS_SHIFT),
			      (uint8_t)(RTL836x_PORT_MISC_EGRESS_ORIGINAL << RTL836x_PORT_MISC_EGRESS_SHIFT));
		reg_write_m(RTL836x_PORT_MISC_CFG_REG(i));
	}
}

/** Get the ingress VLAN filtering status */
bool port_ingress_vlan_filter_get(uint8_t port) __banked
{
	if (port > 7)
		return false;

	return reg_bit_test(RTL836x_VLAN_INGRESS_REG, port) ? true : false;
}
#endif

#if SOC_FAMILY != RTL836X
// C1 bit 0: static (no aging); age at C3[4:2] must be non-zero or the
// entry is invisible to lookup
static void l2_mgmt_entry_fill(__xdata uint8_t *mac, __xdata uint16_t vlan)
{
	REG_WRITE(RTL837x_TBL_DATA_IN_A, mac[2], mac[3], mac[4], mac[5]);
	REG_WRITE(RTL837x_TBL_DATA_IN_B, 0x20 | ((CPU_PORT & 0x3) << 6) | (vlan >> 8),
		  vlan & 0xff, mac[0], mac[1]);
	REG_WRITE(RTL837x_TBL_DATA_IN_C, 0x00, 0x01, 0x00, (7 << 2) | (CPU_PORT >> 2));
}

static void l2_mgmt_tbl_prepare(void)
{
	reg_read_m(RTL837x_TBL_DATA_0);
	sfr_data[2] &= 0x3f;
	sfr_data[1] &= 0xf8;
	reg_write_m(RTL837x_TBL_DATA_0);
}
#endif	/* SOC_FAMILY != RTL836X */

#if SOC_FAMILY == RTL836X
static void rtl836x_l2_mgmt_words(__xdata uint8_t *mac, uint8_t pin)
{
	rtl836x_l2_word[RTL836x_TA_L2_W0] = (uint16_t)(((uint16_t)mac[4] << 8) | mac[5]);
	rtl836x_l2_word[RTL836x_TA_L2_W1] = (uint16_t)(((uint16_t)mac[2] << 8) | mac[3]);
	rtl836x_l2_word[RTL836x_TA_L2_W2] = (uint16_t)(((uint16_t)mac[0] << 8) | mac[1]);
	rtl836x_l2_word[RTL836x_TA_L2_W3] = 0;	/* FID 0 -- see above */
	rtl836x_l2_word[RTL836x_TA_L2_W4] = pin
		? (uint16_t)(RTL836x_TA_L2_W4_AGE_MAX << RTL836x_TA_L2_W4_AGE_SHIFT) : 0;
	rtl836x_l2_word[RTL836x_TA_L2_W5] = pin ? RTL836x_TA_L2_W5_LIVE : 0;
	rtl836x_l2_word[RTL836x_TA_L2_W6] = 0;	/* reads back 0 on this die */
}
#endif	/* SOC_FAMILY == RTL836X (management L2 entry) */

/** Pin the management MAC to the CPU port; remove_entry deletes it */
void port_l2_static_mgmt(__xdata uint8_t *mac, __xdata uint16_t vlan, __xdata bool remove_entry) __banked
{
#if SOC_FAMILY == RTL836X
	(void)vlan;	/* the FID is 0 here -- see rtl836x_l2_mgmt_words() */

	rtl836x_l2_mgmt_words(mac, remove_entry ? 0 : 1);
	rtl836x_l2_write_key();

	rtl836x_l2_mgmt_words(mac, remove_entry ? 0 : 1);
	REG_SET(RTL836x_TA_CTRL_REG, RTL836x_TA_CTRL_L2_READ_MAC);
	rtl836x_tbl_settle();
	reg_read_m(RTL836x_TA_LUT_REG);
	rtl836x_l2_mgmt_status = rtl836x_sfr_u16();
	print_string(remove_entry ? "\nmgmt L2 entry cleared"
				  : "\nmgmt L2 entry written");
	print_string(", look-up status ");
	print_short(rtl836x_l2_mgmt_status);
	if (!(rtl836x_l2_mgmt_status & RTL836x_TA_LUT_FOUND))
		print_string(" (the fabric reports no such entry)");
	write_char('\n');
#else
	wait_table_ready();

	// vlan 0 = management VLAN disabled: untagged management resolves in
	// VLAN 1; clients in other VLANs keep their learned entry and age as before
	if (!vlan)
		vlan = 1;
	l2_mgmt_entry_fill(mac, vlan);
	l2_mgmt_tbl_prepare();
	if (remove_entry) {
		// a search miss returns index 0, not a slot
		REG_WRITE(RTL837X_TBL_CTRL, 0x00, 0x00, TBL_L2_UNICAST, TBL_EXECUTE);
		wait_table_ready();
		reg_read_m(RTL837x_TBL_DATA_0);
		if (!(sfr_data[2] & 0x10))
			return;
		sfr_data[1] |= 0x04; // CLEAR-entry
		reg_write_m(RTL837x_TBL_DATA_0);
		__xdata uint16_t idx = (((uint16_t)sfr_data[2] & 0x0f) << 8) | sfr_data[3];
		REG_WRITE(RTL837X_TBL_CTRL, idx >> 8, idx & 0xff, TBL_L2_UNICAST, TBL_WRITE | TBL_EXECUTE);
	} else {
		// method-0 write: the hardware hashes the key and places the entry
		REG_WRITE(RTL837X_TBL_CTRL, 0x00, 0x00, TBL_L2_UNICAST, TBL_WRITE | TBL_EXECUTE);
	}

	wait_table_ready();
#endif	/* SOC_FAMILY != RTL836X */
}

#if !defined(RTLP_HOST_TEST)

#if SOC_FAMILY == RTL836X

static __xdata uint16_t nic_xmem;		/* XMEM side of the next transfer */
static __xdata uint16_t nic_asic;		/* ASIC side (byte address) */
static __xdata uint8_t nic_units;		/* transfer length / 8 */
static __xdata uint8_t nic_write;		/* 1 = CPU->ASIC */
static __xdata uint16_t nic_guard;		/* bounded-wait counter */
static __xdata uint8_t nic_i;			/* shared scratch loop counter */

extern __xdata uint8_t rx_headers[16];
extern __xdata bool frame_tagged;
extern __xdata uint16_t uip_len;

/* Smallest thing that can be a real Ethernet frame. */
#define NIC8370_MIN_FRAME	14

#define NIC8370_RX_TAIL_UNITS_MAX ((uint8_t)((UIP_CONF_BUFFER_SIZE - 18) >> 3))

#define NIC8370_TX_SETTLE	64

static __xdata uint16_t nic_units_ptr;	/* ring pointer as read: 8-byte units */
static __xdata uint16_t nic_bytes;	/* the same position in bytes */
static __xdata uint16_t nic_len;	/* received frame length */
static __xdata uint16_t nic_rx_ok;	/* frames handed to the dispatch */
static __xdata uint16_t nic_rx_rej;	/* descriptors refused (length / DMA) */
static __xdata uint16_t nic_rx_clamp;	/* tail copies clamped to uip_buf */
static __xdata uint16_t nic_tx_ok;	/* records the ASIC consumed */
static __xdata uint16_t nic_tx_stall;	/* records it left sitting at CTXCPCR */
static __xdata uint16_t nic_tx_before;	/* CTXCPCR before the last staged record */
static __xdata uint16_t nic_reg_hi;	/* HIGH half of the pair being read */
static __xdata uint8_t nic_hi;		/* scratch: value of that half */
static __xdata uint16_t nic_wait;	/* ticks waited for the 0x1a00 block */

static bool nic8370_dma(void)
{
	SFR_NIC_DATA_U16LE = nic_xmem;
	SFR_NIC_RING_U16LE = nic_write ? (nic_asic | 0x8000) : nic_asic;
	SFR_NIC_CTRL = nic_units;
	nic_guard = 0;
	while (SFR_NIC_CTRL != 0) {
		if (++nic_guard == 0)
			return false;	/* engine wedged: caller acks and moves on */
	}
	return true;
}

static void nic8370_pos(uint16_t reg_lo)
{
	reg_read_m(nic_reg_hi);
	nic_hi = sfr_data[3];
	reg_read_m(reg_lo);

	nic_units_ptr = ((uint16_t) nic_hi << 8) | sfr_data[3];
	nic_bytes = nic_units_ptr << 3;
}

void nic8370_init(void) __banked
{
	for (nic_wait = 0; nic_wait < 100; nic_wait++) {
		reg_read_m(RTL836x_REG_NIC_GCR);
		if (sfr_data[3] & 0x10)
			break;
		sleep(1);			/* one 5 ms tick, 100 tries = 0.5 s max */
	}
	print_string("NIC 0x1a00: GCR 0x");
	print_byte(sfr_data[2]);
	print_byte(sfr_data[3]);
	print_string(" wait=");
	print_short(nic_wait);
	if (nic_wait >= 100)
		print_string(" TIMEOUT, still in reset?");
	write_char('\n');

	REG_SET(RTL836x_REG_NIC_RXCMDR, 0x0000);	/* no pending command */
	REG_SET(RTL836x_REG_NIC_TXCMDR, 0x0000);

	REG_SET(RTL836x_REG_NIC_RXSTOPRL, 0x00ff);
	REG_SET(RTL836x_REG_NIC_RXSTOPRH, 0x0002);
	REG_SET(RTL836x_REG_NIC_TXSTOPRL, 0x00fe);
	REG_SET(RTL836x_REG_NIC_TXSTOPRH, 0x0003);

	REG_SET(0x1a48, 0x0004);
	REG_SET(RTL836x_REG_NIC_GCR, 0x0004);
	REG_SET(0x1a5c, 0x0001);

	REG_SET(RTL836x_REG_NIC_IMR, 0x0080);	/* stock: RXIS only */
	REG_SET(RTL836x_REG_NIC_TXCR, 0x0001);	/* TXENABLE */
	REG_SET(RTL836x_REG_NIC_RXCR1, 0x0003);	/* RMCRC + RXENABLE */

	REG_SET(RTL836x_REG_CPU_CTRL, 0x00bb);		/* CPU_EN + TRAP_PORT 7 */
	REG_SET(RTL836x_REG_CPU_PORT_MASK, 0x0080);	/* BIT(7) = port 7 */
	REG_SET(0x13c4, 0x1016);			/* extif2 force up */
	REG_SET(0x131c, 0x1016);			/* CPU-port peer MAC force up */
	REG_SET(0x0800, 0x0080);			/* stock RMA_CTRL00 */

	print_string("NIC 0x1a00 up: RX @");
	nic_reg_hi = RTL836x_REG_NIC_CRXCPRH;
	nic8370_pos(RTL836x_REG_NIC_CRXCPRL);
	print_short(nic_bytes);
	print_string(" TX @");
	nic_reg_hi = RTL836x_REG_NIC_CTXPCRH;
	nic8370_pos(RTL836x_REG_NIC_CTXCPRL);
	print_short(nic_bytes);
	print_string(" (ASIC byte addresses)\n");
}

static void nic8370_ack(void)
{
	REG_SET(RTL836x_REG_NIC_RXCMDR, 0x0001);
	REG_SET(RTL836x_REG_NIC_IMS, 0x0080);
}

bool nic8370_rx_fetch(void) __banked
{
	nic_reg_hi = RTL836x_REG_NIC_RXRDRH;
	nic8370_pos(RTL836x_REG_NIC_RXRDRL);
	if (nic_units_ptr == 0)
		return false;

	/* The 8-byte header sits at our current ring position. */
	nic_reg_hi = RTL836x_REG_NIC_CRXCPRH;
	nic8370_pos(RTL836x_REG_NIC_CRXCPRL);
	nic_xmem = (uint16_t) &rx_headers[0];
	nic_asic = nic_bytes;
	nic_units = 1;
	nic_write = 0;
	if (!nic8370_dma())
		return false;	/* un-acked: see the function comment */

	nic_len = ((uint16_t) rx_headers[6] << 8) | rx_headers[7];
	if (nic_len < NIC8370_MIN_FRAME || nic_len > UIP_CONF_BUFFER_SIZE) {
		nic_rx_rej++;
		return false;	/* un-acked: this is the cursor-destroying case */
	}

	nic_xmem = (uint16_t) &uip_buf[0];
	nic_asic = nic_bytes + 8;
	nic_units = 2;			/* ether header (14) + 2 spare bytes */
	if (!nic8370_dma())
		return false;	/* un-acked: the length was never confirmed */

	nic_xmem = (uint16_t) &uip_buf[20];
	nic_asic = nic_bytes + 16;	/* frame + 8 */
	nic_units = (uint8_t) ((nic_len - 1) >> 3);
	if (nic_units > NIC8370_RX_TAIL_UNITS_MAX) {
		nic_units = NIC8370_RX_TAIL_UNITS_MAX;
		nic_rx_clamp++;
	}
	if (!nic8370_dma())
		return false;	/* un-acked: the length was never confirmed */

	nic8370_ack();
	nic_rx_ok++;
	return true;
}

void nic8370_tx(void) __banked
{
	/* The RTL836x NIC wants the frame length at descriptor offset 6, so stage
	 * it there; uip_len is the frame length before the dot1Q tag is added. */
	if (frame_tagged) {
		uip_buf[6] = uip_buf[5];
		uip_buf[7] = uip_buf[4];
	} else {
		uip_buf[10] = uip_buf[9];
		uip_buf[11] = uip_buf[8];
	}

	nic_reg_hi = RTL836x_REG_NIC_CTXPCRH;
	nic8370_pos(RTL836x_REG_NIC_CTXCPRL);
	nic_tx_before = nic_units_ptr;
	/* A tagged frame starts at uip_buf; an untagged one keeps four bytes of
	 * padding in front, which the NIC does not transmit. */
	if (frame_tagged)
		nic_tx_packet(uip_buf, uip_len + VLAN_TAG_SIZE, nic_units_ptr);
	else
		nic_tx_packet(uip_buf + VLAN_TAG_SIZE, uip_len, nic_units_ptr);

	reg_read_m(RTL836x_REG_NIC_STXCURPKTRL);

	REG_SET(RTL836x_REG_NIC_TXCMDR, 0x0001);

	nic_guard = 0;
	while (nic_guard < NIC8370_TX_SETTLE) {
		nic_reg_hi = RTL836x_REG_NIC_CTXPCRH;
		nic8370_pos(RTL836x_REG_NIC_CTXCPRL);
		if (nic_units_ptr != nic_tx_before)
			break;
		nic_guard++;
	}
	if (nic_guard >= NIC8370_TX_SETTLE)
		nic_tx_stall++;
	else
		nic_tx_ok++;
}

void nic8370_status(void) __banked
{
	print_string("NIC 0x1a00 status:\n");
	print_string("  IMS   ");
	reg_read_m(RTL836x_REG_NIC_IMS);
	print_sfr_data();
	print_string((sfr_data[3] & 0x80) ? " RXIS=1" : " RXIS=0");
	print_string("  IMR   ");
	reg_read_m(RTL836x_REG_NIC_IMR);
	print_sfr_data();
	write_char('\n');
	print_string("  RXCR0 ");
	reg_read_m(RTL836x_REG_NIC_RXCR0);
	print_sfr_data();
	print_string("  RXCR1 ");
	reg_read_m(RTL836x_REG_NIC_RXCR1);
	print_sfr_data();
	write_char('\n');
	print_string("  TXCR  ");
	reg_read_m(RTL836x_REG_NIC_TXCR);
	print_sfr_data();
	print_string("  GCR   ");
	reg_read_m(RTL836x_REG_NIC_GCR);
	print_sfr_data();
	write_char('\n');
	print_string("  RXRDR   ");
	nic_reg_hi = RTL836x_REG_NIC_RXRDRH;
	nic8370_pos(RTL836x_REG_NIC_RXRDRL);
	print_short(nic_units_ptr);
	print_string("=");
	print_short(nic_bytes);
	print_string("B  SRXCUR ");
	nic_reg_hi = RTL836x_REG_NIC_SRXCURPKTRH;
	nic8370_pos(RTL836x_REG_NIC_SRXCURPKTRL);
	print_short(nic_units_ptr);
	print_string("=");
	print_short(nic_bytes);
	print_string("B\n  CRXCPR  ");
	nic_reg_hi = RTL836x_REG_NIC_CRXCPRH;
	nic8370_pos(RTL836x_REG_NIC_CRXCPRL);
	print_short(nic_units_ptr);
	print_string("=");
	print_short(nic_bytes);
	print_string("B\n  CTXCPR  ");
	nic_reg_hi = RTL836x_REG_NIC_CTXPCRH;
	nic8370_pos(RTL836x_REG_NIC_CTXCPRL);
	print_short(nic_units_ptr);
	print_string("=");
	print_short(nic_bytes);
	print_string("B  STXCUR ");
	nic_reg_hi = RTL836x_REG_NIC_STXCURPKTRH;
	nic8370_pos(RTL836x_REG_NIC_STXCURPKTRL);
	print_short(nic_units_ptr);
	print_string("=");
	print_short(nic_bytes);
	print_string("B\n");
	print_string("  RXMBTR  ");
	nic_reg_hi = RTL836x_REG_NIC_RXMBTRH;
	nic8370_pos(RTL836x_REG_NIC_RXMBTRL);
	print_short(nic_bytes);
	print_string("B  RXMPTR ");
	reg_read_m(RTL836x_REG_NIC_RXMPTR);
	print_sfr_data();
	write_char('\n');
	print_string("  last RX header: ");
	for (nic_i = 0; nic_i < 8; nic_i++) {
		print_byte(rx_headers[nic_i]);
		write_char(' ');
	}
	write_char('\n');
	print_string("  rxj ");
	print_short(nic_rx_ok);
	print_string(" rxr ");
	print_short(nic_rx_rej);
	print_string(" rxc ");
	print_short(nic_rx_clamp);
	print_string("  txj ");
	print_short(nic_tx_ok);
	print_string(" txs ");
	print_short(nic_tx_stall);
	write_char('\n');
}

#endif	/* SOC_FAMILY == RTL836X */

#endif	/* !RTLP_HOST_TEST */

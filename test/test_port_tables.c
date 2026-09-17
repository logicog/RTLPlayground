/*
 * test_port_tables.c - rtl837x_port.c against the simulated table engine.
 *
 * What the firmware writes to the VLAN and L2 tables is checked against an
 * independent statement of the entry layout (hw_mock.h), and what it reads
 * back is checked against what it wrote. The module is compiled unmodified.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_port.h"
#include "machine.h"
#include "support.h"
#include "hw_mock.h"

extern struct machine_runtime machine_detected;

/* VLAN entry as vlan_create() lays it out: bit 25 valid, bits 0-9 members,
 * bits 10-19 the untag set, which is every port not in the tagged set (the
 * bit of a non-member is meaningless to the switch and stays set). */
static uint32_t vlan_word(uint16_t members, uint16_t tagged)
{
	return (1u << 25) | ((uint32_t)(~tagged & 0x3ff) << 10) | members;
}

static void t_vlan_roundtrip(void)
{
	printf("[test] VLAN entry: create, read back, delete\n");
	hw_reset(); out_reset();
	vlan_settings.vlan = 10;
	vlan_settings.members = 0x0015;	/* ports 1, 3, 5 */
	vlan_settings.tagged = 0x0004;	/* port 3 tagged */
	vlan_create();
	/* the CPU port is always a tagged member */
	CHECK(hw_vlan_word(10) == vlan_word(0x0015 | 0x0200, 0x0004 | 0x0200), "vlan 10 entry has the documented layout");
	CHECK(hw_vlan_word(11) == 0, "the neighbouring entry is untouched");

	uint32_t w = hw_vlan_word(10);
	CHECK(vlan_get(10) == 0, "vlan_get finds it");
	CHECK(((uint32_t)sfr_data[0] << 24 | (uint32_t)sfr_data[1] << 16 | (uint32_t)sfr_data[2] << 8 | sfr_data[3]) == w,
	      "and returns the word that was written");
	CHECK(sfr_data[0] & 0x02, "with the valid bit the JSON generators test for");

	unsigned long r = hw_reads;
	CHECK(vlan_get(4095) == -1, "VLAN 4095 is refused");
	CHECK(hw_reads == r, "without touching the engine");

	vlan_delete(10);
	CHECK(hw_vlan_word(10) == 0, "vlan_delete clears the entry");
	vlan_delete(0);
	vlan_delete(4095);
	CHECK(hw_vlan_word(0) == 0 && hw_vlan_word(4095) == 0, "VLAN 0 and 4095 cannot be deleted");
}

static void t_vlan_rtl8372(void)
{
	printf("[test] VLAN entry on an RTL8372: ports 0-2 are not there\n");
	hw_reset();
	machine_detected.isRTL8373 = 0;
	vlan_settings.vlan = 20;
	vlan_settings.members = 0x03ff;
	vlan_settings.tagged = 0x0000;
	vlan_create();
	uint32_t w = hw_vlan_word(20);
	CHECK(((w >> 10) & 0x7) == 0, "the untagged bits of ports 0-2 stay clear");
	CHECK((w & 0x3ff) == 0x03ff, "the member bits are what was asked for");
	machine_detected.isRTL8373 = 1;
}

static void t_pvid(void)
{
	printf("[test] PVID: two ports share one register\n");
	hw_reset();
	port_pvid_set(2, 100);
	port_pvid_set(3, 4094);
	CHECK(port_pvid_get(2) == 100, "even port reads back");
	CHECK(port_pvid_get(3) == 4094, "odd port reads back");
	port_pvid_set(3, 7);
	CHECK(port_pvid_get(2) == 100, "writing the odd port leaves the even one alone");
	CHECK(port_pvid_get(3) == 7, "and takes the new value");
	port_pvid_set(2, 4001);
	CHECK(port_pvid_get(3) == 7, "writing the even port leaves the odd one alone");
	CHECK(port_pvid_get(8) == 0, "an untouched port reads 0");
}

static void t_l2mc(void)
{
	printf("[test] static multicast entry steering a group to the CPU\n");
	hw_reset();
	port_l2mc_set(0x0e, 2, PMASK_CPU);
	const uint8_t mac[6] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e };
	const struct hw_l2 *e = hw_l2_find(mac, 2);
	CHECK(e != 0, "the entry exists for 01:80:c2:00:00:0e in VLAN 2");
	if (e) {
		uint8_t m[6]; uint16_t vid, pm; bool valid, st, mc;
		hw_l2_decode(e, m, &vid, &pm, &valid, &st, &mc);
		CHECK(valid, "valid");
		CHECK(mc, "multicast");
		CHECK(pm == PMASK_CPU, "member mask is the CPU port alone");
	}
	port_l2mc_set(0x0e, 2, PMASK_CPU | 0x0003);
	e = hw_l2_find(mac, 2);
	CHECK(hw_l2_count() == 1, "writing the same key again replaces, not adds");
	if (e) {
		uint8_t m[6]; uint16_t vid, pm; bool valid, st, mc;
		hw_l2_decode(e, m, &vid, &pm, &valid, &st, &mc);
		CHECK(pm == (PMASK_CPU | 0x0003), "with the new mask, low bits included");
	}
}

static void t_static_mgmt(void)
{
	printf("[test] the management MAC pinned to the CPU port\n");
	hw_reset();
	uint8_t mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
	port_l2_static_mgmt(mac, 2, false);
	const struct hw_l2 *e = hw_l2_find(mac, 2);
	CHECK(e != 0, "entry present in VLAN 2");
	if (e) {
		uint8_t m[6]; uint16_t vid, p; bool valid, st, mc;
		hw_l2_decode(e, m, &vid, &p, &valid, &st, &mc);
		CHECK(valid && st && !mc, "valid, static, unicast");
		CHECK(p == CPU_PORT, "on the CPU port");
	}
	port_l2_static_mgmt(mac, 0, false);
	CHECK(hw_l2_find(mac, 1) != 0, "management VLAN 0 means VLAN 1");
	port_l2_static_mgmt(mac, 2, true);
	CHECK(hw_l2_find(mac, 2) == 0, "removal deletes the VLAN 2 entry");
	CHECK(hw_l2_find(mac, 1) != 0, "and leaves the other one");
	port_l2_static_mgmt(mac, 3, true);
	CHECK(hw_l2_count() == 1, "removing an entry that is not there changes nothing");
}

static void t_flush(void)
{
	printf("[test] flushing a port forgets its learned entries only\n");
	hw_reset();
	const uint8_t a[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x01 };
	const uint8_t b[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x02 };
	const uint8_t c[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x03 };
	hw_l2_put(0x010, a, 1, 4, false);
	hw_l2_put(0x020, b, 1, 4, true);
	hw_l2_put(0x030, c, 1, 5, false);
	port_l2_forget_port(4);
	CHECK(hw_l2_find(a, 1) == 0, "learned entry on port 5 (index 4) gone");
	CHECK(hw_l2_find(b, 1) != 0, "static entry on the same port stays");
	CHECK(hw_l2_find(c, 1) != 0, "entry on another port stays");
	port_l2_forget();
	CHECK(hw_l2_find(c, 1) == 0 && hw_l2_find(b, 1) != 0, "forget-all drops every learned entry and keeps the static one");
}

static void t_lag(void)
{
	printf("[test] trunk membership and its hash seed\n");
	hw_reset();
	port_lag_members_set(1, 0x0180);
	CHECK(port_lag_members_get(1) == 0x0180, "members read back");
	CHECK(port_lag_of(7) == 1 && port_lag_of(8) == 1, "both ports belong to group 2");
	CHECK(port_lag_of(0) == PORT_LAG_NONE, "a port outside is in no group");
	CHECK((hw_reg_get(RTL837X_TRK_HASH_CTRL_BASE + 4) & 0xff) == LAG_HASH_DEFAULT, "an unset hash gets the default");
	hw_reg_set(RTL837X_TRK_HASH_CTRL_BASE + 8, 0x06);
	port_lag_members_set(2, 0x0060);
	CHECK((hw_reg_get(RTL837X_TRK_HASH_CTRL_BASE + 8) & 0xff) == 0x06, "a hash already set is kept");
}

int main(void)
{
	printf("== rtl837x_port.c table tests ==\n");
	t_vlan_roundtrip();
	t_vlan_rtl8372();
	t_pvid();
	t_l2mc();
	t_static_mgmt();
	t_flush();
	t_lag();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

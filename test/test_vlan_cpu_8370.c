/*
 * test_vlan_cpu_8370.c - the CPU port's bit in the fabric masks, and the
 * 11-bit VLAN member mask it lives in.
 *
 * Why this needs a test of its own: on this die the CPU/ring NIC hangs off
 * fabric port 10. The die reports chip number 0x6368 at 0x1300, which the
 * vendor SDK resolves to CHIP_RTL8370B -- and its HAL for that chip
 * (rtk_switch.c, rtl8370b_hal_Ctrl) gives eight UTP ports at physical 0..7,
 * three extension ports at 8/9/10, "Maximum physical port number 10" and
 * "Valid CPU port mask (0x1 << EXT_PORT2)": the CPU port is physical 10.
 * Measured on the board, after the extif force the CPU link is in PORTn_STATUS
 * at 0x135c = 0x0016 while 0x1359 reads 0x0896, front port 8's own link.
 *
 * The tree used to call port 7 the CPU port. That is front port 8 -- the eighth
 * UTP port -- so the "CPU bit" ORed into every VLAN member mask was the eighth
 * port's own bit: `vlan 5 1 2` programmed members 0x83 (port 8 forced in
 * whether asked for or not), and `isolate <port> CPU` granted access to front
 * port 8 instead of the CPU. The mock records every register write, so both
 * are visible here.
 *
 * What the checks pin down: the CPU bit is 0x400, outside the eight front-port
 * bits; the CVLAN member/untag fields are 11 bits SPLIT across table words 0
 * and 2, so the CPU's bit is written to word 2 (bit 2 = member port 10, bit 5
 * = untag port 10) and never to word 0; a VLAN's front-port membership is
 * exactly what the user asked for, with the CPU added as a tagged member; and
 * the port-isolation register (an 11-bit field) carries the CPU at bit 10 while
 * fabric bit 7 stays front port 8.
 *
 * This is one of the host binaries built with -DMACHINE_NAME=SODOLA_SL_SG008W, so
 * machine.h derives SOC_FAMILY=RTL836X and the RTL836x paths are taken.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtlos_regs.h"
#include "rtl837x_port.h"
#include "machine.h"
#include "support.h"
#include "hw_mock.h"

void delay(uint16_t ms) { (void)ms; }
extern __code const struct machine machine;

static uint16_t written_u16(uint16_t addr)
{
	return (uint16_t)(hw_reg_get(addr) >> 16);
}

static uint16_t staged_vlan_w0(void)
{
	return written_u16(RTL836x_TA_WRDATA_REG(0));
}

static uint16_t staged_vlan_w2(void)
{
	return written_u16(RTL836x_TA_WRDATA_REG(2));
}

static void t_cpu_port_bit(void)
{
	printf("[test] the CPU port is fabric port 10, outside the eight front ports\n");

	CHECK(CPU_PORT == 10, "CPU_PORT is 10, not the old 7");
	CHECK(((uint16_t)1 << CPU_PORT) == 0x400,
	      "so the CPU port's bit is 0x400");
	CHECK((((uint16_t)1 << CPU_PORT) & 0xff) == 0,
	      "which is outside the eight front-port bits");
	CHECK((((uint16_t)1 << CPU_PORT) & (1 << 7)) == 0,
	      "and bit 7 stays front port 8 -- the collision the old value caused");
}

static void t_vlan_create_members(void)
{
	printf("[test] a VLAN's member mask is what was asked for, plus nothing\n");
	hw_reset();
	out_reset();

	vlan_settings.vlan = 5;
	vlan_settings.members = 0x03;
	vlan_settings.tagged = 0x01;
	vlan_create();

	CHECK(staged_vlan_w0() == 0x0203,
	      "members 0x03 with untag 0x02 -- front port 8 is not in the VLAN");
	CHECK((staged_vlan_w0() & 0x0080) == 0, "so the eighth UTP port's bit is clear");
	CHECK(staged_vlan_w2() == 0x0004,
	      "word 2 carries the CPU port as a member (bit 2 = member port 10)");
	CHECK((staged_vlan_w2() & 0x0020) == 0,
	      "and not as untagged -- the CPU is a tagged member");
	CHECK(written_u16(RTL836x_TA_ADDR_REG) == 5, "the entry is written at the VID");
	CHECK(written_u16(RTL836x_TA_CTRL_REG) == RTL836x_TA_CTRL_CVLAN_WRITE,
	      "with the CVLAN write control word");

	hw_reset();
	out_reset();
	vlan_settings.vlan = 5;
	vlan_settings.members = 0x03 | PMASK_CPU;
	vlan_settings.tagged = 0;
	vlan_create();

	CHECK(staged_vlan_w0() == 0x0303,
	      "'members 1 2 CPU' programs ports 1 and 2 and nothing else in word 0");
	CHECK((staged_vlan_w0() & 0x0080) == 0, "and still not front port 8");
	CHECK(staged_vlan_w2() == 0x0004, "with the CPU member bit in word 2");
}

static void t_isolation_cpu_bit(void)
{
	printf("[test] port isolation carries the CPU at bit 10, next to bit 7\n");
	hw_reset();
	out_reset();

	port_isolate(1, PMASK_CPU);
	CHECK(written_u16(RTL836x_PORT_ISOLATION_REG(1)) == 0x400,
	      "'isolate 2 CPU' grants the CPU port's bit, 0x400");
	CHECK(written_u16(RTL836x_PORT_ISOLATION_REG(1) ) != 0x080,
	      "not front port 8's bit, which is what the old value wrote");

	port_isolate(1, PMASK_CPU | 1);
	CHECK(written_u16(RTL836x_PORT_ISOLATION_REG(1)) == 0x401,
	      "the CPU bit combines with ordinary port bits");

	port_isolate(2, 0x80);
	CHECK(written_u16(RTL836x_PORT_ISOLATION_REG(2)) == 0x080,
	      "front port 8 is an ordinary port bit in the same field");

	/* Reading back: the getter must not report front port 8 as the CPU. */
	hw_reset();
	hw_reg_set(RTL836x_PORT_ISOLATION_REG(1), 0x0400);
	CHECK(port_isolation_get(1) == PMASK_CPU, "a CPU-only mask reads back as the CPU");

	hw_reg_set(RTL836x_PORT_ISOLATION_REG(1), 0x0080);
	CHECK(port_isolation_get(1) == 0x080,
	      "a mask holding front port 8 does not read back as the CPU");
	CHECK(port_isolation_get(1) != PMASK_CPU, "which is the bug the report hit");

	hw_reg_set(RTL836x_PORT_ISOLATION_REG(1), 0x0481);
	CHECK(port_isolation_get(1) == ((uint16_t)0x81 | PMASK_CPU),
	      "and both together read back as both");
}

static void t_l2_setup_isolation(void)
{
	printf("[test] l2 setup opens every port, CPU port included, at bit 10\n");
	hw_reset();
	out_reset();
	port_l2_setup();

	CHECK(written_u16(RTL836x_PORT_ISOLATION_REG(0)) == 0x7ff,
	      "the mask is the register's reset value -- extension bits and CPU, not 0x4ff");
	CHECK((written_u16(RTL836x_PORT_ISOLATION_REG(0)) & 0x080) != 0,
	      "front port 8 is in it as itself, not because it was mistaken for the CPU");
	CHECK(written_u16(RTL836x_PORT_ISOLATION_REG(7)) == 0x7ff,
	      "and every port gets the same mask");

	CHECK((0x7ff & RTL836x_MEMBER_MASK) == 0xff,
	      "word 0's member byte holds the eight front ports alone");
	CHECK(RTL836x_MEMBER_MASK_WIDE == 0x7ff,
	      "while the member field proper is 11 bits -- the CPU is bit 10");
}

static void t_ingress_accept_frame_type(void)
{
	uint16_t reg0 = RTL836x_VLAN_ACCEPT_FRAME_TYPE_REG(0);

	printf("[test] `ingress` writes the per-port frame type at 0x07aa\n");
	hw_reset();
	out_reset();

	CHECK(RTL836x_VLAN_ACCEPT_FRAME_TYPE_REG(1) == 0x07aa,
	      "ports 0-7 share one register at 0x07aa");
	CHECK(RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(1) == 2 &&
	      RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(4) == 8 &&
	      RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(7) == 14,
	      "with two bits per port, low ports first");

	/* Port 0, the low field: tagged only. */
	CHECK(port_ingress_filter(0, VLAN_TAGGED), "`ingress 1 t` is accepted");
	CHECK(hw_reg_get(reg0) == 0x0001, "port 0's field holds 1 = tagged only");

	/* A port further up: the field is shifted, not just low bits. */
	hw_reset();
	out_reset();
	CHECK(port_ingress_filter(3, VLAN_UNTAGGED), "`ingress 4 u` is accepted");
	CHECK(hw_reg_get(reg0) == 0x0080, "port 3's field (shift 6) holds 2 = untagged only");

	hw_reset();
	out_reset();
	CHECK(port_ingress_filter(7, VLAN_UNTAGGED), "the last front port is addressable");
	CHECK(hw_reg_get(reg0) == 0x8000, "its field is the top two bits");

	hw_reset();
	out_reset();
	hw_reg_set(reg0, (uint16_t)(VLAN_TAGGED << RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(7)));
	CHECK(port_ingress_filter(1, VLAN_UNTAGGED), "`ingress 2 u` is accepted");
	CHECK(hw_reg_get(reg0) ==
	      (uint16_t)(0x4000 | (VLAN_UNTAGGED << RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(1))),
	      "port 7's setting survives and port 1 gains its own");

	/* 'a' clears the field back to "any", the reset value. */
	hw_reset();
	out_reset();
	hw_reg_set(reg0, 0x0003);
	CHECK(port_ingress_filter(0, VLAN_ALL), "`ingress 1 a` is accepted");
	CHECK(hw_reg_get(reg0) == 0x0000, "and clears port 0's frame type");

	/* Refusals: no ninth UTP port on this die, and no fourth mode. */
	hw_reset();
	out_reset();
	CHECK(!port_ingress_filter(8, VLAN_TAGGED), "port 8 is outside the eight front ports");
	CHECK(!port_ingress_filter(0, VLAN_INVALID), "VLAN_INVALID is not a frame type");
	CHECK(hw_reg_get(reg0) == 0 && hw_reg_get(RTL836x_VLAN_ACCEPT_FRAME_TYPE_REG(8)) == 0,
	      "and neither refusal writes a register");

	/* The getter is what `vlan show` prints, so it must read the same field. */
	hw_reset();
	hw_reg_set(reg0, 0);
	CHECK(port_ingress_filter_get(0) == VLAN_ALL, "an untouched port reads back as 'any'");

	hw_reg_set(reg0, (uint16_t)(VLAN_TAGGED << RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(1)));
	CHECK(port_ingress_filter_get(1) == VLAN_TAGGED, "tagged only reads back tagged only");
	CHECK(port_ingress_filter_get(0) == VLAN_ALL, "next to a port that was never set");

	hw_reg_set(reg0, (uint16_t)((VLAN_TAGGED << RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(4)) |
				    (VLAN_UNTAGGED << RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(6))));
	CHECK(port_ingress_filter_get(4) == VLAN_TAGGED, "two fields in one register decode separately");
	CHECK(port_ingress_filter_get(6) == VLAN_UNTAGGED, "by their own shift");
	CHECK(port_ingress_filter_get(8) == VLAN_INVALID, "and port 8 has no field to read");
}

static void t_boot_pvid_default_unset(void)
{
	printf("[test] boot-time PVID 1 default, without arming 0x07a8\n");
	hw_reset();
	out_reset();

	/* A fresh chip: every port on MC index 0, which is empty. */
	port_pvid_default_unset();

	CHECK(staged_vlan_w0() == 0xffff,
	      "VLAN 1's 4K entry is created, all eight ports member and untagged");
	CHECK(staged_vlan_w2() == 0x0024,
	      "with the CPU port in word 2 -- member bit 2 and untag bit 5");
	CHECK(written_u16(RTL836x_TA_ADDR_REG) == 1, "at VID 1");
	CHECK((written_u16(RTL836x_VLAN_MC_BASE(1)) & ((uint16_t)1 << CPU_PORT)) != 0,
	      "the MC entry for VLAN 1 names the CPU port's bit");
	CHECK(written_u16(RTL836x_VLAN_MC_BASE(1) + 3) == 1, "and carries EVID 1");
	CHECK((written_u16(RTL836x_VLAN_PVID_CTRL_REG(0)) >> 8) == 1,
	      "the unset UTP ports are repointed onto it");
	CHECK((written_u16(RTL836x_VLAN_PVID_CTRL_REG(3)) >> 8) == 1,
	      "out to the last pair");
	CHECK(written_u16(RTL836x_VLAN_PVID_CTRL_REG(CPU_PORT)) == 1,
	      "and the CPU port (0x0705) gets it too");

	CHECK(hw_reg_get(RTL836x_VLAN_CTRL_REG) == 0,
	      "but the global 0x07a8 gate is never written by the boot rule");
	CHECK(hw_reg_get(RTL836x_VLAN_INGRESS_REG) == 0,
	      "and no per-port 0x07a9 bit is set either");

	/* A port the configuration already gave a real PVID keeps it. */
	hw_reset();
	out_reset();
	hw_reg_set(RTL836x_VLAN_MC_BASE(2), 0xff);	/* port 0 already on VLAN 20 */
	hw_reg_set(RTL836x_VLAN_MC_BASE(2) + 3, 20);
	hw_reg_set(RTL836x_VLAN_PVID_CTRL_REG(0), 2);

	port_pvid_default_unset();

	CHECK((written_u16(RTL836x_VLAN_PVID_CTRL_REG(0)) & 0x1f) == 2,
	      "a replayed `pvid` wins and is not dragged onto VLAN 1");
	CHECK((written_u16(RTL836x_VLAN_PVID_CTRL_REG(0)) >> 8) == 1,
	      "while the unset port sharing its register still defaults");
	CHECK(hw_reg_get(RTL836x_VLAN_CTRL_REG) == 0, "still without arming 0x07a8");

	hw_reset();
	out_reset();
	{
		uint8_t p;
		for (p = 0; p <= CPU_PORT; p++) {
			hw_reg_set(RTL836x_VLAN_MC_BASE(3), 0xff);
			hw_reg_set(RTL836x_VLAN_MC_BASE(3) + 3, 30);
			hw_reg_set(RTL836x_VLAN_PVID_CTRL_REG(p), 0x0303);
		}
	}
	port_pvid_default_unset();
	CHECK(written_u16(RTL836x_TA_ADDR_REG) == 1,
	      "the default VLAN 1 is ensured even when no port needs a PVID");
	CHECK(staged_vlan_w2() == 0x0024,
	      "and names the CPU as member and untagged, so 0x07a8 is safe");
	CHECK(written_u16(RTL836x_VLAN_PVID_CTRL_REG(0)) == 0,
	      "while not one PVID register is rewritten");
	CHECK(hw_reg_get(RTL836x_VLAN_CTRL_REG) == 0,
	      "and the global gate is still off");
}

static void t_vlan_filter_default_on(void)
{
	printf("[test] VLAN filtering is armed by default, like the RTL8372/3\n");
	hw_reset();
	out_reset();

	port_vlan_filter_default_on();
	CHECK(written_u16(RTL836x_VLAN_INGRESS_REG) == RTL836x_MEMBER_MASK,
	      "0x07a9 filters all eight UTP ports");
	CHECK(hw_reg_get(RTL836x_VLAN_CTRL_REG) == 0x0001,
	      "and the global 0x07a8 gate is on");

	hw_reset();
	out_reset();
	port_pvid_default_unset();
	port_vlan_filter_default_on();
	CHECK(staged_vlan_w2() == 0x0024,
	      "VLAN 1 names the CPU as a member, so the gate cannot starve it");
	CHECK(written_u16(RTL836x_VLAN_INGRESS_REG) == RTL836x_MEMBER_MASK,
	      "and all eight UTP ports come up filtering");
	CHECK(hw_reg_get(RTL836x_VLAN_CTRL_REG) == 0x0001, "with the global gate on");
}

static void t_vlan_egress_original(void)
{
	uint8_t p;
	bool all = true;

	printf("[test] front ports strip a CPU-inserted tag: egress mode ORIGINAL\n");
	hw_reset();
	out_reset();

	/* The chip's reset value, and what every port reads on the live board. */
	for (p = MACHINE_MIN_PORT; p <= MACHINE_MAX_PORT; p++)
		hw_reg_set(RTL836x_PORT_MISC_CFG_REG(p), 0x48b0);

	port_vlan_egress_original();

	for (p = MACHINE_MIN_PORT; p <= MACHINE_MAX_PORT; p++) {
		if (hw_reg_get(RTL836x_PORT_MISC_CFG_REG(p)) != 0x4880)
			all = false;
	}
	CHECK(all, "0x48b0 -> 0x4880 on every front port, the rest of the register kept");
	CHECK(((0x4880u >> RTL836x_PORT_MISC_EGRESS_SHIFT) & RTL836x_PORT_MISC_EGRESS_MASK)
	      == RTL836x_PORT_MISC_EGRESS_ORIGINAL,
	      "and bits 5:4 read back as the ORIGINAL constant");

	/* A real MISC value with the bandwidth flags set keeps them. */
	hw_reset();
	out_reset();
	hw_reg_set(RTL836x_PORT_MISC_CFG_REG(0), 0x4cb0);
	port_vlan_egress_original();
	CHECK(hw_reg_get(RTL836x_PORT_MISC_CFG_REG(0)) == 0x4c80,
	      "bits 11:10 survive the read-modify-write");
}

static void t_vlan_show_cpu_row(void)
{
	printf("[test] `vlan show` prints the CPU port's PVID row\n");
	hw_reset();
	out_reset();

	hw_reg_set(RTL836x_VLAN_MC_BASE(1), 0xff);
	hw_reg_set(RTL836x_VLAN_MC_BASE(1) + 3, 1);
	hw_reg_set(RTL836x_VLAN_PVID_CTRL_REG(CPU_PORT), 1);
	CHECK(port_pvid_get(CPU_PORT) == 1,
	      "the CPU port's PVID reads back through port_pvid_get()");

	/* Its accept-frame-type field is the one at (port 10) in 0x07ab. */
	hw_reg_set(RTL836x_VLAN_ACCEPT_FRAME_TYPE_REG(CPU_PORT),
		   (uint16_t)(VLAN_TAGGED << RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(CPU_PORT)));
	CHECK(port_ingress_filter_get(CPU_PORT) == VLAN_TAGGED,
	      "and the CPU port's own ingress frame-type field decodes");
	/* Neighbours in the shared register are unaffected by that decode. */
	CHECK(port_ingress_filter_get(8) == VLAN_INVALID,
	      "while fabric port 8 still has no readable field");

	out_reset();
	vlan_dump();
	CHECK(strstr(out_buf, "CPU\t0001\tTagged") != NULL,
	      "`vlan show` prints a CPU row: PVID 1, its frame type, Filtering");

	/* A different PVID for the CPU port shows in the row too. */
	hw_reset();
	out_reset();
	hw_reg_set(RTL836x_VLAN_MC_BASE(5), 0xff);
	hw_reg_set(RTL836x_VLAN_MC_BASE(5) + 3, 30);
	hw_reg_set(RTL836x_VLAN_PVID_CTRL_REG(CPU_PORT), 5);
	vlan_dump();
	CHECK(strstr(out_buf, "CPU\t001e") != NULL,
	      "and follows the CPU port's PVID rather than hardcoding 1");
}

static void t_vlan_delete_repoints_pvid(void)
{
	printf("[test] `vlan 10 d` moves a port whose PVID was 10 onto VLAN 1\n");
	hw_reset();
	out_reset();

	hw_reg_set(RTL836x_VLAN_MC_BASE(1), 0xff);		/* default VLAN 1 */
	hw_reg_set(RTL836x_VLAN_MC_BASE(1) + 3, 1);
	hw_reg_set(RTL836x_VLAN_MC_BASE(2), 0xff);		/* the VLAN being deleted */
	hw_reg_set(RTL836x_VLAN_MC_BASE(2) + 3, 10);
	hw_reg_set(RTL836x_VLAN_PVID_CTRL_REG(0), 2);	/* ports 0 and 1 */
	hw_reg_set(RTL836x_VLAN_PVID_CTRL_REG(2), 2);	/* ports 2 and 3: shared entry */

	vlan_delete(10);

	CHECK(staged_vlan_w0() == 0, "the 4K entry for VLAN 10 is cleared");
	CHECK(written_u16(RTL836x_VLAN_MC_BASE(2) + 3) == 0,
	      "and the MC entry it used is emptied rather than recycled");
	CHECK((written_u16(RTL836x_VLAN_PVID_CTRL_REG(0)) & 0x1f) == 1,
	      "port 0 is repointed to the VID-1 entry, not left on the emptied slot");
	CHECK((written_u16(RTL836x_VLAN_PVID_CTRL_REG(0)) >> 8) == 0,
	      "port 1 shares that register, was on entry 0, and is not repointed");
	CHECK(written_u16(RTL836x_VLAN_PVID_CTRL_REG(2)) == 0x0001,
	      "port 2 shared the emptied entry with port 0 and moves with it");

	hw_reset();
	out_reset();
	hw_reg_set(RTL836x_VLAN_MC_BASE(5), 0xff);		/* port 0's own PVID */
	hw_reg_set(RTL836x_VLAN_MC_BASE(5) + 3, 20);
	hw_reg_set(RTL836x_VLAN_MC_BASE(7), 0xff);		/* VLAN 10, unused */
	hw_reg_set(RTL836x_VLAN_MC_BASE(7) + 3, 10);
	hw_reg_set(RTL836x_VLAN_PVID_CTRL_REG(0), 5);

	vlan_delete(10);

	CHECK(written_u16(RTL836x_VLAN_PVID_CTRL_REG(0)) == 0,
	      "deleting a VLAN no port uses as its PVID writes no PVID register");
	CHECK(written_u16(RTL836x_VLAN_MC_BASE(7) + 3) == 0,
	      "while that VLAN's entry is still cleared");
}

/* `isolate <port> off` is parse_isolate()'s member mask: every front port plus
 * the CPU bit. The register is written verbatim -- there is no separate
 * "restore the reset value" path on this die. */
static void t_isolate_off_masks_front_and_cpu(void)
{
	printf("[test] `isolate <port> off` admits every front port plus the CPU\n");
	hw_reset();
	out_reset();

	port_isolate(0, (uint16_t)(0xff | PMASK_CPU));
	CHECK(written_u16(RTL836x_PORT_ISOLATION_REG(0)) == 0x4ff,
	      "all eight front ports plus the CPU's bit 10 are written");
	CHECK((written_u16(RTL836x_PORT_ISOLATION_REG(0)) & 0x300) == 0,
	      "and the two unnamed extension ports stay out of the mask");

	hw_reset();
	hw_reg_set(RTL836x_PORT_ISOLATION_REG(0), 0x7ff);
	CHECK(port_isolation_get(0) == 0x7ff,
	      "the read side hands back the raw 11-bit field");

	hw_reset();
	out_reset();
	port_isolate(1, (uint16_t)(0xff | PMASK_CPU));
	CHECK(written_u16(RTL836x_PORT_ISOLATION_REG(1)) == 0x4ff,
	      "an explicit all-ports+CPU mask is written as 0x4ff");

	hw_reset();
	out_reset();
	port_isolate((uint8_t)(MACHINE_MAX_PORT + 1), (uint16_t)(0xff | PMASK_CPU));
	CHECK(hw_reg_get(RTL836x_PORT_ISOLATION_REG(MACHINE_MAX_PORT + 1)) == 0,
	      "a port above MACHINE_MAX_PORT writes nothing");
}

static void t_vlan4k_word2_packing(void)
{
	printf("[test] the 4K member mask splits across word 0 and word 2\n");
	hw_reset();
	out_reset();

	vlan_settings.vlan = 42;
	vlan_settings.members = 0;
	vlan_settings.tagged = 0;
	vlan_create();
	CHECK(staged_vlan_w0() == 0x0000, "no front port is a member, so word 0 is 0");
	CHECK(staged_vlan_w2() == 0x0004, "the CPU is the only member, in word 2");
	CHECK(written_u16(RTL836x_TA_WRDATA_REG(1)) == 0, "word 1 stays the FID word");

	hw_reset();
	out_reset();
	hw_reg_set(RTL836x_TA_RDDATA_REG(0), 0x0000);
	hw_reg_set(RTL836x_TA_RDDATA_REG(1), 0x0000);
	hw_reg_set(RTL836x_TA_RDDATA_REG(2), 0x0004);
	CHECK(vlan_get(7) == 0, "an entry whose only member is the CPU reads valid");
	CHECK(sfr_data[0] == 0x02, "with the web UI's validity byte set");

	/* And an entry with no member in either word still reads empty. */
	hw_reset();
	out_reset();
	hw_reg_set(RTL836x_TA_RDDATA_REG(2), 0x0000);
	CHECK(vlan_get(7) == -1, "an entry with no member in either word reads empty");

	CHECK(RTL836x_TA_VLAN2_MEMBER_HI_SHIFT == 0 && RTL836x_TA_VLAN2_UNTAG_HI_SHIFT == 3,
	      "member high bits at 2:0 and untag high bits at 5:3, per the SDK");
}

static void t_ingress_vlan_filter(void)
{
	uint16_t reg = RTL836x_VLAN_INGRESS_REG;

	printf("[test] `ingress vlan filter` pins the per-port bit in 0x07a9\n");
	hw_reset();

	CHECK(port_ingress_vlan_filter_set(3, true), "enabling port 3 is accepted");
	CHECK((hw_reg_get(reg) & 0x0008) != 0, "and sets port 3's bit in the ingress register");
	CHECK(port_ingress_vlan_filter_get(3), "the getter reads the same bit");
	CHECK(hw_reg_get(reg) == 0x0008, "and nothing else moved");

	CHECK(port_ingress_vlan_filter_set(7, true), "port 7 as well");
	CHECK(hw_reg_get(reg) == 0x0088, "so two ports can be filtered together");
	CHECK(port_ingress_vlan_filter_get(7) && !port_ingress_vlan_filter_get(2),
	      "while port 2 stays unfiltered");

	CHECK(port_ingress_vlan_filter_set(3, false), "disabling port 3 is accepted");
	CHECK(port_ingress_vlan_filter_get(3) == false, "and its bit is gone");
	CHECK(hw_reg_get(reg) == 0x0080, "port 7's bit untouched");

	/* refusals: the ninth front port does not exist on this die */
	CHECK(!port_ingress_vlan_filter_set(8, true), "port 8 is rejected");
	CHECK(!port_ingress_vlan_filter_get(8), "and its getter reports nothing");
	CHECK(hw_reg_get(reg) == 0x0080, "with no register written");
}

int main(void)
{
	printf("== rtl837x_port.c RTL836x VLAN/CPU-port bit tests ==\n");
	t_cpu_port_bit();
	t_vlan_create_members();
	t_vlan4k_word2_packing();
	t_isolation_cpu_bit();
	t_l2_setup_isolation();
	t_ingress_accept_frame_type();
	t_ingress_vlan_filter();
	t_boot_pvid_default_unset();
	t_vlan_filter_default_on();
	t_vlan_egress_original();
	t_vlan_show_cpu_row();
	t_isolate_off_masks_front_and_cpu();
	t_vlan_delete_repoints_pvid();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

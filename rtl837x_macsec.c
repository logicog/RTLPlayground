#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_macsec.h"
#include "aes128.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];

#define MACSEC_POLL_LIMIT	1000

static __xdata uint8_t macsec_ports;
static __xdata uint8_t ms_port;
static __xdata uint8_t ms_v[4];
static __xdata uint8_t ms_block[AES128_BLOCK_LEN];
static __xdata uint8_t ms_sci[MACSEC_SCI_LEN];


static uint16_t ms_reg(uint16_t reg)
{
	return reg + ((uint16_t)(ms_port - MACSEC_FIRST_PORT) << MACSEC_PORT_STRIDE);
}


static void ms_set32(uint32_t v)
{
	ms_v[0] = v >> 24;
	ms_v[1] = v >> 16;
	ms_v[2] = v >> 8;
	ms_v[3] = v;
}


static void ms_write(uint16_t reg)
{
	sfr_data[0] = ms_v[0];
	sfr_data[1] = ms_v[1];
	sfr_data[2] = ms_v[2];
	sfr_data[3] = ms_v[3];
	reg_write_m(ms_reg(reg));
}


static void ms_write32(uint16_t reg, __xdata uint32_t v)
{
	ms_set32(v);
	ms_write(reg);
}


static void ms_read(uint16_t reg)
{
	reg_read_m(ms_reg(reg));
}


static void ms_poll(__xdata uint16_t reg, __xdata uint8_t byte, __xdata uint8_t mask, __xdata uint8_t want)
{
	__xdata uint16_t n = MACSEC_POLL_LIMIT;

	do {
		ms_read(reg);
	} while ((sfr_data[byte] & mask) != want && --n);
}


static void eng_write_v(uint8_t dir, __xdata uint16_t addr)
{
	__xdata uint8_t b0 = ms_v[0], b1 = ms_v[1], b2 = ms_v[2], b3 = ms_v[3];

	if (dir == MACSEC_EGRESS) {
		ms_write32(RTL837X_MACSEC_ADDR_AE, (uint32_t)addr << 16);
		ms_v[0] = b2; ms_v[1] = b3; ms_v[2] = b0; ms_v[3] = b1;
		ms_write(RTL837X_MACSEC_RWDH_AE);
		ms_write32(RTL837X_MACSEC_CMD_AE, MACSEC_CMD_AE_WR_REQ);
	} else {
		ms_v[2] = 0; ms_v[3] = 0;
		ms_write(RTL837X_MACSEC_CMD_AE);
		ms_v[0] = 0; ms_v[1] = 0; ms_v[2] = b2; ms_v[3] = b3;
		ms_write(RTL837X_MACSEC_RWDL_AI);
		ms_write32(RTL837X_MACSEC_ADDR_AI, MACSEC_ADDR_AI_WR_REQ | addr);
	}
}


static void eng_write(uint8_t dir, __xdata uint16_t addr, __xdata uint32_t v)
{
	ms_set32(v);
	eng_write_v(dir, addr);
}


static void eng_write_le(uint8_t dir, __xdata uint16_t addr, __xdata const uint8_t * __xdata p)
{
	ms_v[0] = p[3];
	ms_v[1] = p[2];
	ms_v[2] = p[1];
	ms_v[3] = p[0];
	eng_write_v(dir, addr);
}


static void eng_read(uint8_t dir, __xdata uint16_t addr)
{
	if (dir == MACSEC_EGRESS) {
		ms_write32(RTL837X_MACSEC_ADDR_AE, (uint32_t)addr << 16);
		ms_write32(RTL837X_MACSEC_CMD_AE, MACSEC_CMD_AE_RD_REQ);
		ms_poll(RTL837X_MACSEC_CMD_AE, 3, MACSEC_CMD_AE_RD_REQ, 0);
		ms_read(RTL837X_MACSEC_RWDH_AE);
		ms_v[0] = sfr_data[2];
		ms_v[1] = sfr_data[3];
		ms_v[2] = sfr_data[0];
		ms_v[3] = sfr_data[1];
	} else {
		ms_write32(RTL837X_MACSEC_ADDR_AI, MACSEC_ADDR_AI_RD_REQ | addr);
		ms_poll(RTL837X_MACSEC_ADDR_AI, 1, MACSEC_ADDR_AI_RD_REQ >> 16, 0);
		ms_read(RTL837X_MACSEC_CMD_AE);
		ms_v[0] = sfr_data[0];
		ms_v[1] = sfr_data[1];
		ms_read(RTL837X_MACSEC_RWDL_AI);
		ms_v[2] = sfr_data[2];
		ms_v[3] = sfr_data[3];
	}
}


static uint8_t eng_present(uint8_t dir)
{
	eng_read(dir, EIP160_VERSION);
	return ms_v[2] == (EIP160_VERSION_ID >> 8) && ms_v[3] == (EIP160_VERSION_ID & 0xff);
}


static void ms_glb_set1(uint32_t v)
{
	ms_read(RTL837X_MACSEC_GLB_SET1);
	sfr_data[0] |= MACSEC_GLB_SET1_GATING >> 24;
	reg_write_m(ms_reg(RTL837X_MACSEC_GLB_SET1));
	ms_poll(RTL837X_MACSEC_GLB_MASK, 2, MACSEC_GLB_MASK_BITS, MACSEC_GLB_MASK_BITS);
	ms_poll(RTL837X_MACSEC_GLB_MASK, 3, MACSEC_GLB_MASK_BITS, MACSEC_GLB_MASK_BITS);
	ms_write32(RTL837X_MACSEC_GLB_SET1, v | MACSEC_GLB_SET1_GATING);
	ms_write32(RTL837X_MACSEC_GLB_SET1, v);
	ms_poll(RTL837X_MACSEC_GLB_MASK, 2, MACSEC_GLB_MASK_BITS, 0);
	ms_poll(RTL837X_MACSEC_GLB_MASK, 3, MACSEC_GLB_MASK_BITS, 0);
}


static void eng_record(uint8_t dir, __xdata const uint8_t * __xdata key, __xdata const uint8_t * __xdata sci)
{
	uint8_t i;

	for (i = 0; i < AES128_BLOCK_LEN; i++)
		ms_block[i] = 0;
	aes128_encrypt(key, ms_block);

	eng_write(dir, EIP160_TR_WORD(EIP160_TR_CTRL), dir == MACSEC_EGRESS ? EIP160_CTRL_EGRESS : EIP160_CTRL_INGRESS);
	eng_write(dir, EIP160_TR_WORD(EIP160_TR_CTX_ID), dir == MACSEC_EGRESS ? EIP160_CTX_ID_EGRESS : EIP160_CTX_ID_INGRESS);
	for (i = 0; i < MACSEC_KEY_LEN; i += 4) {
		eng_write_le(dir, EIP160_TR_WORD(EIP160_TR_KEY) + i, key + i);
		eng_write_le(dir, EIP160_TR_WORD(EIP160_TR_HKEY) + i, ms_block + i);
	}
	eng_write(dir, EIP160_TR_WORD(EIP160_TR_SEQ), EIP160_FIRST_PN);
	if (dir == MACSEC_EGRESS) {
		eng_write_le(dir, EIP160_TR_WORD(EIP160_TR_EGRESS_IV), sci);
		eng_write_le(dir, EIP160_TR_WORD(EIP160_TR_EGRESS_IV + 1), sci + 4);
	} else {
		eng_write(dir, EIP160_TR_WORD(EIP160_TR_INGRESS_MASK), 0);
		eng_write_le(dir, EIP160_TR_WORD(EIP160_TR_INGRESS_IV), sci);
		eng_write_le(dir, EIP160_TR_WORD(EIP160_TR_INGRESS_IV + 1), sci + 4);
	}
}


static void eng_init(uint8_t dir)
{
	uint8_t i;

	eng_write(dir, EIP160_CTX_CTRL, dir == MACSEC_EGRESS ? EIP160_CTX_CTRL_EGRESS : EIP160_CTX_CTRL_INGRESS);
	eng_write(dir, EIP160_CTX_UPD_CTRL, EIP160_CTX_UPD_CTRL_ALL);
	eng_write(dir, EIP160_MISC_CONTROL, dir == MACSEC_EGRESS ? EIP160_MISC_CONTROL_EGRESS : EIP160_MISC_CONTROL_INGRESS);
	eng_write(dir, EIP160_NM_FLOW_NCP, dir == MACSEC_EGRESS ? EIP160_NM_DROP_EGRESS : EIP160_NM_DROP_INGRESS);
	eng_write(dir, EIP160_NM_FLOW_CP, dir == MACSEC_EGRESS ? EIP160_NM_DROP_EGRESS : EIP160_NM_DROP_INGRESS);
	eng_write(dir, EIP160_SAM_ENTRY_CLEAR, EIP160_SAM_ENTRY_0);
	eng_write(dir, EIP160_SAM_FLOW_CTRL, 0);
	for (i = 0; i < EIP160_TR_WORDS; i++)
		eng_write(dir, EIP160_TR_WORD(i), 0);
}


static void own_sci(void)
{
	uint8_t i;

	for (i = 0; i < 6; i++)
		ms_sci[i] = uip_ethaddr.addr[i];
	ms_sci[6] = 0;
	ms_sci[7] = machine.log_to_phys_port[ms_port];
}


static uint8_t macsec_port_ok(uint8_t port)
{
	if (port >= MACSEC_FIRST_PORT && port <= MACSEC_LAST_PORT)
		return 1;
	print_string("no MACsec on port ");
	print_phys_port(port);
	write_char('\n');
	return 0;
}


void macsec_on(uint8_t port, __xdata const uint8_t * __xdata key, __xdata const uint8_t * __xdata peer_sci) __banked
{
	if (!macsec_port_ok(port))
		return;
	ms_port = port;
	ms_write32(RTL837X_MACSEC_PM_CTRL, MACSEC_PM_CTRL_CLOCKS);
	if (!eng_present(MACSEC_EGRESS) || !eng_present(MACSEC_INGRESS)) {
		ms_write32(RTL837X_MACSEC_PM_CTRL, 0);
		print_string("MACsec engine not found\n");
		return;
	}

	eng_init(MACSEC_EGRESS);
	eng_init(MACSEC_INGRESS);

	own_sci();
	eng_record(MACSEC_EGRESS, key, ms_sci);
	eng_write(MACSEC_EGRESS, EIP160_SAM_MISC, EIP160_SAM_MISC_UNTAGGED);
	eng_write(MACSEC_EGRESS, EIP160_SAM_MASK, 0);
	eng_write(MACSEC_EGRESS, EIP160_SAM_FLOW_CTRL, EIP160_FLOW_TX);
	eng_write(MACSEC_EGRESS, EIP160_SAM_ENTRY_SET, EIP160_SAM_ENTRY_0);

	eng_record(MACSEC_INGRESS, key, peer_sci);
	eng_write(MACSEC_INGRESS, EIP160_SAM_MISC, EIP160_SAM_MISC_TAGGED | EIP160_SAM_MISC_TCI_SC);
	eng_write_le(MACSEC_INGRESS, EIP160_SAM_SCI_LO, peer_sci);
	eng_write_le(MACSEC_INGRESS, EIP160_SAM_SCI_HI, peer_sci + 4);
	eng_write(MACSEC_INGRESS, EIP160_SAM_MASK, EIP160_SAM_MASK_SCI | EIP160_SAM_MASK_AN | EIP160_SAM_MASK_TCI_SC);
	eng_write(MACSEC_INGRESS, EIP160_SAM_FLOW_CTRL, EIP160_FLOW_RX);
	eng_write(MACSEC_INGRESS, EIP160_SAM_ENTRY_SET, EIP160_SAM_ENTRY_0);

	ms_glb_set1(MACSEC_GLB_SET1_RUN);
	macsec_ports |= 1 << port;
}


void macsec_off(uint8_t port) __banked
{
	if (!macsec_port_ok(port))
		return;
	ms_port = port;
	ms_write32(RTL837X_MACSEC_PM_CTRL, MACSEC_PM_CTRL_CLOCKS);
	ms_glb_set1(MACSEC_GLB_SET1_RESET);
	eng_init(MACSEC_EGRESS);
	eng_init(MACSEC_INGRESS);
	ms_write32(RTL837X_MACSEC_PM_CTRL, 0);
	macsec_ports &= ~(1 << port);
}


static void show_counter(__code const char *name, __xdata uint8_t dir, __xdata uint16_t addr)
{
	print_string(name);
	eng_read(dir, addr);
	print_byte(ms_v[0]); print_byte(ms_v[1]); print_byte(ms_v[2]); print_byte(ms_v[3]);
}


void macsec_show(void) __banked
{
	uint8_t i;

	for (ms_port = MACSEC_FIRST_PORT; ms_port <= MACSEC_LAST_PORT; ms_port++) {
		if (!(macsec_ports & (1 << ms_port)))
			continue;
		own_sci();
		print_string("port ");
		print_phys_port(ms_port);
		print_string(" sci ");
		for (i = 0; i < MACSEC_SCI_LEN; i++)
			print_byte(ms_sci[i]);
		show_counter(" tx pn ", MACSEC_EGRESS, EIP160_TR_WORD(EIP160_TR_SEQ));
		show_counter(" tx enc ", MACSEC_EGRESS, EIP160_OUT_PKTS_ENCRYPTED);
		show_counter(" rx ok ", MACSEC_INGRESS, EIP160_IN_PKTS_OK);
		show_counter(" bad ", MACSEC_INGRESS, EIP160_IN_PKTS_NOT_VALID);
		show_counter(" notag ", MACSEC_INGRESS, EIP160_IN_PKTS_NO_TAG);
		show_counter(" nosci ", MACSEC_INGRESS, EIP160_IN_PKTS_NO_SCI);
		write_char('\n');
	}
}

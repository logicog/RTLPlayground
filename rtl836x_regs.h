#ifndef _RTL836X_REGS_H_
#define _RTL836X_REGS_H_

/*
 * RTL836x register map: the register definitions and the register-access
 * macros of this die only.
 *
 * Included through rtlos_regs.h, which owns the dispatch and picks this
 * header or rtl837x_regs.h per SOC_FAMILY -- the same way rtlos_phy.h picks
 * the PHY driver header. Nothing on the RTL8372/3 sees these definitions, and
 * this header holds no RTL8372/3 register.
 *
 * The register-access macros further down are this die's own as well: the
 * RTL836x register data is 16 bits wide in SFR A4 (high byte) and A5 (low),
 * where the RTL8372/3 use all four bytes A4..A7 (see reg_write_m()).
 */

/*
 * Register map. Source: Realtek Linux SDK (api_8370 / RTL836x headers) plus
 * the RTL8367RB/RTL8367MB register table. Where marked, verified against the
 * stock firmware dump and/or live silicon. The RTL8372/3 registers live in
 * rtl837x_regs.h: they are a DIFFERENT die, do not use them here without
 * verification.
 */
#define RTL836x_REG_RESET		0x1322
// CHIP_RESET: bit5=NIC_RST bit4=DW8051_RST bit2=CONFIG_RST bit1=SW_RST bit0=CHIP_RST
#define RTL836x_REG_MISCELLANEOUS_CONFIGURE0 0x130c
// bit12=FLASH_ENABLE bit10=NIC_ENABLE bit5=DW8051_EN bit4=AUTOLOAD_EN
#define RTL836x_REG_CHIP_ID		0x1300
#define RTL836x_REG_CHIP_VER		0x1301
// bits 15:12 = package type (rtk_switch_init dispatch)
#define RTL836x_REG_CHIP_MODE		0x1302
// bits 2:0 = chip mode (patch-table selector)
#define RTL836x_REG_MAGIC_ID		0x13c2
#define RTL836x_MAGIC_UNLOCK		0x0249
// internal PHY window: addr = 0x2000 | (phy << 5) | phyreg
#define RTL836x_PHY_BASE		0x2000
#define RTL836x_REG_LED_SYS_CONFIG	0x1b00
#define RTL836x_REG_PARA_LED_IO_EN1	0x1b24	/* group0 low byte, group1 high */
#define RTL836x_REG_PARA_LED_IO_EN2	0x1b25	/* group2 low byte */
#define RTL836x_REG_LED_ACTIVE_LOW_CFG0	0x1b0e
#define RTL836x_REG_LED_ACTIVE_LOW_CFG1	0x1b0f
#define RTL836x_REG_PHY_AD		0x130f
#define RTL836x_REG_LUT_PORT0_LEARN_LIMITNO	0x0a20
// one register per port, ascending

/*
 * NIC Related registers
 *
 * ATTENTION: the 0x78xx block of rtl837x_regs.h is the RTL8372/3 descriptor
 * DMA -- PHANTOM on the RTL836x. This die has the classic RTL836x-family ring
 * NIC at 0x1a00 (SDK rtl836x_reg.h: "(16'h1a00)nic_reg"), full map below.
 * Stock firmware never addresses it via the engine (no IP stack); the ring is
 * configured by AUTOLOAD hardware tables (GCR 0x1a17 = 0x0010 at boot = out
 * of reset, RXMTU set). Ring-driver port in rtl836x_nic.c.
 */
#define RTL836x_REG_NIC_RXRDRL		0x1a04	/* RX ring desc read ptr L */
#define RTL836x_REG_NIC_RXRDRH		0x1a05	/* RX ring desc read ptr H */
#define RTL836x_REG_NIC_RXCMDR		0x1a0c	/* bit0: RX fetch cmd */
#define RTL836x_REG_NIC_TXCMDR		0x1a0d	/* bit0: TX start cmd */
#define RTL836x_REG_NIC_IMS		0x1a0e	/* intr status: RXIS b7, TXIS b6, TXES b5, RXBUS b3, TXBOS b2, RXMIS b1, TXNLS b0 */
#define RTL836x_REG_NIC_IMR		0x1a0f	/* intr mask, same layout as IMS */
#define RTL836x_REG_NIC_RXCR0		0x1a14	/* RX frame-class enables: HFPPE b7, HFMPE b6, RXBPE b5, RXMPE b4, RXPPS b3:2, RXAPE b1, ARPPE b0 */
#define RTL836x_REG_NIC_RXCR1		0x1a15	/* RL4CEPE b4, RL3CEPE b3, RCRCEPE b2, RMCRC b1, RXENABLE b0 */
#define RTL836x_REG_NIC_TXCR		0x1a16	/* LBE b2 (loopback!), TXMFM b1, TXENABLE b0 */
#define RTL836x_REG_NIC_GCR		0x1a17	/* RXMTU b5:4, SRST b0 */
#define RTL836x_REG_NIC_TXSTOPRL	0x1a44	/* TX ring stop L */
#define RTL836x_REG_NIC_TXSTOPRH	0x1a45	/* TX ring stop H (mask 0x7) */
#define RTL836x_REG_NIC_RXSTOPRL	0x1a46	/* RX ring stop L */
#define RTL836x_REG_NIC_RXSTOPRH	0x1a47	/* RX ring stop H (mask 0x7) */
#define RTL836x_REG_NIC_RXMBTRL	0x1a4c	/* RX memory base L */
#define RTL836x_REG_NIC_RXMBTRH	0x1a4d	/* RX memory base H (mask 0x7f) */
#define RTL836x_REG_NIC_RXMPTR		0x1a4e	/* RX memory write cursor */
#define RTL836x_REG_NIC_CRXCPRL	0x1a50	/* cur RX CPU ptr L (CPU advances this) */
#define RTL836x_REG_NIC_CRXCPRH	0x1a51	/* cur RX CPU ptr H */
#define RTL836x_REG_NIC_CTXCPRL	0x1a52	/* cur TX CPU ptr L */
#define RTL836x_REG_NIC_CTXPCRH	0x1a53	/* cur TX CPU ptr H */
#define RTL836x_REG_NIC_SRXCURPKTRL	0x1a54	/* shadow cur RX pkt L */
#define RTL836x_REG_NIC_SRXCURPKTRH	0x1a55	/* shadow cur RX pkt H */
#define RTL836x_REG_NIC_STXCURPKTRL	0x1a56	/* shadow cur TX pkt L */
#define RTL836x_REG_NIC_STXCURPKTRH	0x1a57	/* shadow cur TX pkt H */
#define RTL836x_REG_CPU_PORT_MASK	0x1219	/* CPU port mask (RTL836x) */
#define RTL836x_REG_CPU_CTRL		0x121a	/* CPU_EN b0, INSERTMODE b2:1, TRAP_PORT b6:3 */

/*
 * Register access. This die's switch-register data is 16 bits wide in SFR A4
 * (high byte) and A5 (low), where the RTL8372/3 use all four bytes A4..A7.
 * rtl837x_regs.h may have been included before us (see rtlos_regs.h), so drop
 * its 32-bit pair before defining this die's.
 */
#undef REG_SET
#undef REG_WRITE

#define SFR_WRITE_U16(v) do { \
	SFR_DATA_24 = (((uint16_t)(v)) >> 8) & 0xff; \
	SFR_DATA_16 = (v) & 0xff; \
	SFR_DATA_8 = 0; \
	SFR_DATA_0 = 0; \
} while (0)
#define SFR_WRITE_BYTES(v24, v16, v8, v0) do { \
	SFR_DATA_24 = (v8); \
	SFR_DATA_16 = (v0); \
	SFR_DATA_8 = 0; \
	SFR_DATA_0 = 0; \
} while (0)

#ifdef REGDBG

#define REG_SET(r, v) do { \
	SFR_WRITE_U16(v); \
	reg_write(r); \
	write_char('R'); print_byte(r >> 8); print_byte(r); write_char('-'); \
	print_byte(((v) >> 24) & 0xff); print_byte((v) >> 16 & 0xff); print_byte((v) >> 8 & 0xff); print_byte( (v) & 0xff); write_char(' '); \
} while (0)

#define	REG_WRITE(r, v24, v16, v8, v0) do { \
	SFR_WRITE_BYTES(v24, v16, v8, v0); \
	reg_write(r); \
	write_char('R'); print_byte(r>>8); print_byte(r); write_char('-'); print_byte(v24); print_byte(v16); print_byte(v8); print_byte(v0); write_char(' '); \
} while (0)
#else
#define REG_SET(r, v) do { \
	SFR_WRITE_U16(v); \
	reg_write(r); \
} while (0)

#define	REG_WRITE(r, v24, v16, v8, v0) do { \
	SFR_WRITE_BYTES(v24, v16, v8, v0); \
	reg_write(r); \
} while (0)
#endif

#define RTL836x_TA_CTRL_REG		0x0500
#define RTL836x_TA_ADDR_REG		0x0501
#define RTL836x_TA_LUT_REG		0x0502
#define RTL836x_TA_WRDATA_REG(x)	(0x0510 + (x))
#define RTL836x_TA_RDDATA_REG(x)	(0x0520 + (x))
#define RTL836x_TA_CTRL_CMD_SHIFT	3
#define RTL836x_TA_CTRL_CMD_WRITE	1
#define RTL836x_TA_CTRL_TABLE_L2	4
#define RTL836x_TA_CTRL_METHOD		(1 << 4)
#define RTL836x_TA_CTRL_CVLAN_READ	3
#define RTL836x_TA_CTRL_CVLAN_WRITE	0x0b
#define RTL836x_TA_CTRL_L2_READ_MAC	(RTL836x_TA_CTRL_TABLE_L2)					/* 0x04 */
#define RTL836x_TA_CTRL_L2_READ_ADDR	(RTL836x_TA_CTRL_METHOD | RTL836x_TA_CTRL_TABLE_L2)	/* 0x14 */
#define RTL836x_TA_CTRL_L2_WRITE \
	((RTL836x_TA_CTRL_CMD_WRITE << RTL836x_TA_CTRL_CMD_SHIFT) | RTL836x_TA_CTRL_TABLE_L2)	/* 0x0c */

#define RTL836x_TA_LUT_FOUND		0x1000	/* lookup answered with an entry */
#define RTL836x_TA_LUT_ADDR_MASK	0x1fff

#define RTL836x_TA_L2_NUM_WORDS	7
#define RTL836x_TA_L2_W0		0
#define RTL836x_TA_L2_W1		1
#define RTL836x_TA_L2_W2		2
#define RTL836x_TA_L2_W3		3
#define RTL836x_TA_L2_W4		4
#define RTL836x_TA_L2_W5		5
#define RTL836x_TA_L2_W6		6
#define RTL836x_TA_L2_W3_FID_MASK	0x0fff
#define RTL836x_TA_L2_W3_STATIC		0x8000
#define RTL836x_TA_L2_W4_AGE_SHIFT	11
#define RTL836x_TA_L2_W4_AGE_MASK	0x1f
#define RTL836x_TA_L2_W4_AGE_MAX		0x06	/* the fabric's fresh/stored age: weights higher were
						 * normalized by the fabric (0x3f00 -> 0x3200) */
#define RTL836x_TA_L2_W4_SPA_SHIFT	8
#define RTL836x_TA_L2_W4_SPA_MASK	0x07
#define RTL836x_TA_L2_W5_LIVE		0x0040	/* set on every entry the fabric wrote */
/* LUT multicast entry: word 4 replaces the unicast entry's age|spa byte with
 * the member mask in the port-frame numbering; bits 8-9 are unused (the die
 * has the eight front ports below them and the CPU at bit 10). Verified with
 * a staged entry that the die keeps verbatim. */
#define RTL836x_TA_L2_W4_MC_MASK	0x07ff
/* LUT spa space of the CPU port: the fabric rewrote the staged invalid spa of
 * the management entry to 2 (read live on the SL-SG008W logging LUT port 2 for
 * the static switch-MAC entry, while all learned LAN MACs were learned on
 * other spa values and per-port learn-count regs agree with the decode). */
#define RTL836x_CPU_LUT_PORT		2
#define RTL836x_LUT_PORT_LEARN_LIMIT_REG(p)	(0x0a20 + (p))
/* Per-port learned-entry count, one register per port (vendor-confirmed
 * address; read live on the SL-SG008W when the fabric was learning). */
#define RTL836x_LUT_PORT_LEARN_CNT_REG(p)	(0x0a87 + (p))
#define RTL836x_LUT_PORT_LEARN_CNT_MASK	0x3fff
#define RTL836x_FORCE_FLUSH_REG	0x0a36
#define RTL836x_L2_FLUSH_ALL		0x00ff
#define RTL836x_PORT_SECURITY_CTRL_REG	0x08c8
#define RTL836x_LUT_LEARN_OVER_ACT_MASK	0x30

#define RTL836x_VID_MASK		0x0fff
#define RTL836x_FID_MASK		0x000f
/* Low-word halves (the member/untag fields' low 8 bits, ports 0-7). */
#define RTL836x_UNTAG_MASK		0x00ff
#define RTL836x_MEMBER_MASK		0x00ff
/* The full 11-bit fields, ports 0-10 (the CPU's bit is 1 << 10). */
#define RTL836x_MEMBER_MASK_WIDE	0x07ff
#define RTL836x_UNTAG_MASK_WIDE	0x07ff
#define RTL836x_TA_VLAN0_UNTAG_SHIFT	8
/* Member/untag bits 10:8 live in word 2 (member bits 2:0, untag bits 5:3). */
#define RTL836x_TA_VLAN_HI_SHIFT	8
#define RTL836x_TA_VLAN_HI_MASK	0x07
#define RTL836x_TA_VLAN2_MEMBER_HI_SHIFT 0
#define RTL836x_TA_VLAN2_UNTAG_HI_SHIFT  3

#define RTL836x_NUM_VLANS		32
#define RTL836x_VLAN_MC_BASE(x)	(0x0728 + ((x) * 4))

/* Per-port PVID register (2 ports packed per 16-bit register, 5 bits each). */
#define RTL836x_VLAN_PVID_CTRL_REG(p)	(0x0700 + ((p) / 2))
#define RTL836x_VLAN_PVID_CTRL_MASK	0x1f
#define RTL836x_VLAN_PVID_CTRL_SHIFT(p)	(8 * ((p) % 2))

/* Global VLAN control and ingress filtering (one bit per port). */
#define RTL836x_VLAN_CTRL_REG		0x07a8
#define RTL836x_VLAN_INGRESS_REG	0x07a9

#define RTL836x_VLAN_ACCEPT_FRAME_TYPE_REG(p)	(0x07aa + ((p) >> 3))
#define RTL836x_VLAN_ACCEPT_FRAME_TYPE_SHIFT(p)	(((p) & 0x7) << 1)

/* Port egress mode (2 bits, 5:4), one register per port. */
#define RTL836x_PORT_MISC_CFG_REG(p)	(0x000e + 0x20 * (p))
#define RTL836x_PORT_MISC_EGRESS_SHIFT	4
#define RTL836x_PORT_MISC_EGRESS_MASK	0x3
#define RTL836x_PORT_MISC_EGRESS_ORIGINAL 0

#define RTL836x_IGBW_RATE_LO_REG(p)	(0x000f + ((p) << 5))
#define RTL836x_IGBW_RATE_HI_REG(p)	(0x0010 + ((p) << 5))
#define RTL836x_IGBW_RATE_MASK		0x1ffff
#define RTL836x_IGBW_RATE_DISABLE	0x1ffff
#define RTL836x_IGBW_MISC_FC_OFFSET	11

#define RTL836x_PORT_ISOLATION_REG(p)	(0x08a2 + (p))

#define RTL836x_PORT_STATUS_REG(p)	(0x1352 + (p))
#define RTL836x_PORT_STATUS_NWAY	0x0080
#define RTL836x_PORT_STATUS_TXPAUSE	0x0040
#define RTL836x_PORT_STATUS_RXPAUSE	0x0020
#define RTL836x_PORT_STATUS_LINK	0x0010
#define RTL836x_PORT_STATUS_DUPLEX	0x0004
#define RTL836x_PORT_STATUS_SPEED_MASK	0x0003
#define RTL836x_PORT_STATUS_SPEED_10	0
#define RTL836x_PORT_STATUS_SPEED_100	1

#define RTL836x_SWC0_REG		0x1200
#define RTL836x_SWC0_MAX_LENGTH_SHIFT	13
#define RTL836x_SWC0_MAX_LENGTH_MASK	(0x3 << 13)

#define RTL836x_MIB_COUNTER_BASE_REG	0x1000
#define RTL836x_MIB_ADDRESS_REG		0x1004
#define RTL836x_MIB_CTRL_REG		0x1005
#define RTL836x_MIB_BUSY_FLAG		0x0001
#define RTL836x_MIB_RESET_FLAG		0x0002
#define RTL836x_MIB_PORT_OFFSET		0x7c
#define RTL836x_MIB_LEARNENTRYDISCARD_OFFSET	0x500
#define RTL836x_MIBS_NUMBER		37

#define RTL836x_MIB_IF_IN_OCTETS	0
#define RTL836x_MIB_DOT3_FCS_ERR	1
#define RTL836x_MIB_DOT3_SYMBOL_ERR	2
#define RTL836x_MIB_DOT3_IN_PAUSE	3
#define RTL836x_MIB_DOT3_CTRL_UNKNOWN	4
#define RTL836x_MIB_ETHER_FRAGMENTS	5
#define RTL836x_MIB_ETHER_JABBERS	6
#define RTL836x_MIB_IF_IN_UCAST		7
#define RTL836x_MIB_ETHER_DROP_EVENTS	8
#define RTL836x_MIB_ETHER_UNDERSIZE	10
#define RTL836x_MIB_ETHER_OVERSIZE	11
#define RTL836x_MIB_ETHER_PKTS_64	12
#define RTL836x_MIB_ETHER_PKTS_65_127	13
#define RTL836x_MIB_ETHER_MCAST		18
#define RTL836x_MIB_ETHER_BCAST		19
#define RTL836x_MIB_IF_OUT_OCTETS	20
#define RTL836x_MIB_DOT3_SINGLE_COLL	21
#define RTL836x_MIB_DOT3_MULTI_COLL	22
#define RTL836x_MIB_DOT3_DEFERRED	23
#define RTL836x_MIB_DOT3_LATE_COLL	24
#define RTL836x_MIB_ETHER_COLLISIONS	25
#define RTL836x_MIB_DOT3_EXCESSIVE_COLL	26
#define RTL836x_MIB_DOT3_OUT_PAUSE	27
#define RTL836x_MIB_IF_OUT_DISCARDS	28
#define RTL836x_MIB_DOT1D_PORT_IN_DISCARDS 29
#define RTL836x_MIB_IF_OUT_UCAST	30
#define RTL836x_MIB_IF_OUT_MCAST	31
#define RTL836x_MIB_IF_OUT_BCAST	32
#define RTL836x_MIB_LEARN_DISCARD	36

#define RTL836x_CPU_EXTIF_FORCE_REG	0x13c4
#define RTL836x_CPU_PEER_FORCE_REG	0x131c
#define RTL836x_CPU_FORCE_VALUE	0x1016
#define RTL836x_RMA_CTRL00_REG		0x0800
#define RTL836x_RMA_CTRL00_VALUE	0x0080

#define RTL836x_PORT_EEE_CFG_REG(p)	(0x0018 + ((p) << 5))
#define RTL836x_EEE_TX_BIT		0x0200	/* b9  EEE_TX  (mac enable eee tx) */
#define RTL836x_EEE_RX_BIT		0x0100	/* b8  EEE_RX  (mac enable eee rx) */
#define RTL836x_EEE_GIGA_BIT		0x0400	/* b10 EEE_GIGA */
#define RTL836x_EEE_100M_BIT		0x0800	/* b11 EEE_100M */
/* Status/telemetry bits in the same register (PDF 16.5 / SDK status struct). */
#define RTL836x_EEE_LPI_BIT		0x0020	/* b5  link is in LPI */

#define RTL836x_EEE_PHY_ENABLE_REG21	0x0100
#define RTL836x_EEE_PHY_DISABLE_REG21	0x0000
#define RTL836x_EEE_PHY_REG6_ENABLE	0x0062
#define RTL836x_EEE_PHY_REG6_DISABLE	0x0042
#define RTL836x_EEE_PHY_REG5		0x8b84
#define RTL836x_EEE_PHY_REG31_PAGE5	5
#define RTL836x_EEE_PHY_REG31_PAGE7	7
#define RTL836x_EEE_PHY_REG31_PAGE0	0
#define RTL836x_EEE_PHY_REG30		30
#define RTL836x_EEE_PHY_REG30_VALUE	32
#define RTL836x_EEE_PHY_REG21		21

#define RTL836x_MIRROR_CTRL_REG	0x121c
#define RTL836x_MIRROR_SRC_PMSK_REG	0x12fb
#define RTL836x_MIRROR_SOURCE_SHIFT	0
#define RTL836x_MIRROR_MONITOR_SHIFT	4
#define RTL836x_MIRROR_RX_BIT		0x0200
#define RTL836x_MIRROR_TX_BIT		0x0400

#define RTL836x_TRUNK_CTRL_REG		0x08cf
#define RTL836x_TRUNK_GROUP_REG	0x08d0
#define RTL836x_TRUNK_GROUP_SHIFT(lag)	(((lag) & 0x3) << 2)
#define RTL836x_TRUNK_GROUP_MASK	0x000f

#define RTL836x_MSTP_STATE_BASE	0x0a00
#define RTL836x_MSTP_STATE_REG(port)	(RTL836x_MSTP_STATE_BASE + ((port) >> 3))

#endif

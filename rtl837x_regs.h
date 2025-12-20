#ifndef _RTL837X_REGS_H_
#define _RTL837X_REGS_H_

#define RTL837X_REG_CHIP_INFO		0x000c
#define RTL837X_REG_RESET		0x0024
#define RESET_SOC_BIT			0
#define RESET_NIC_BIT			2

#define RTL837X_REG_HW_CONF		0x6040
// Bits 4 & 5: CLOCK DIVIDER from 125MHz for Timer

#define RTL837X_REG_LED_MODE		0x6520
// Defines the LED Mode for steering the Port LEDS and the System LED
// BIT 17 set: LED solid on
// Bytes 0/1 hold the LED mode, e.g. serial, RTL8231?
// Blink rate is defined by setAsicRegBits(0x6520,0xe00000,rate);
#define RTL837X_REG_LED_GLB_IO_EN	0x65DC
#define RTL837X_REG_LED3_0_SET1		0x6528
#define RTL837X_REG_LED3_2_SET0		0x6544
#define RTL837X_REG_LED1_0_SET0		0x6548

// SMI control
#define RTL837X_REG_SMI_PORT0_5_ADDR	0x644C
#define RTL837X_REG_SMI_PORT6_9_ADDR	0x6450
#define RTL837X_REG_SMI_CTRL		0x6454
#define RTL837X_REG_SMI_MAC_TYPE	0x6330
#define RTL837X_REG_SMI_PORT_POLLING	0x6334

#define RTL837X_REG_SEC_COUNTER 0x06f4
#define RTL837X_REG_SEC_COUNTER2 0x06f8
// Used for counting seconds

#define RTL837X_REG_SDS_MODES 0x7b20
/*
 * 5 Bits each give the state of the 2 SerDes of the RTL8372
 * Values are:
 */
#define SDS_SGMII		0x02
#define SDS_1000BX_FIBER	0x04
#define SDS_QXGMII		0x0d
#define SDS_HISGMII		0x12
#define SDS_HSG			0x16
#define SDS_10GR		0x1a
#define SDS_OFF			0x1f

#define RTL837X_REG_LINKS	0x63f0
#define RTL837X_REG_LINKS_89	0x63f4
/* Each nibble encodes the link state of a port.
   Port 0 appears to be the CPU port
   The RTL8372 serves ports 4-7, port 3 is the RTL8221
   2: 1Gbit
   5: 2.5Gbit
  */


/*
 * Pin configuration (pinmux)
 */

#define RTL837X_PIN_MUX_0	0x7f8c
#define RTL837X_PIN_MUX_1	0x7f90
#define RTL837X_PIN_MUX_2	0x7f94

// Output Registers
#define RTL837X_REG_GPIO_00_31_OUTPUT 0x3c
#define RTL837X_REG_GPIO_32_63_OUTPUT 0x40
// BIT 4 resets RTL8224 on 9000-9XH

// Input Registers
#define RTL837X_REG_GPIO_00_31_INPUT 0x44
#define RTL837X_REG_GPIO_32_63_INPUT 0x48
// Bit 1e cleared: SFP Module inserted on 9000-6XH (MOD_DEF0 pin)

// BIT 5 set: SIGNAL LOS of SFP module on 9000-6XH (RX_LOS pin)

// Direction Registers, 0 = input, 1 = output
#define RTL837X_REG_GPIO_00_31_DIRECTION 0x4c
#define RTL837X_REG_GPIO_32_63_DIRECTION 0x50

/*
 * I2C controller
 */
#define RTL837X_REG_I2C_MST_IF_CTRL	0x0414
#define RTL837X_REG_I2C_CTRL		0x0418
#define I2C_DEV_ADDR			3
#define I2C_MEM_ADDR_WIDTH		20
#define RTL837X_REG_I2C_CTRL2		0x041c
#define RTL837X_REG_I2C_IN		0x0420
#define RTL837X_REG_I2C_OUT		0x0424

/*
 * NIC Related registers
 */
#define RTL837X_REG_NIC_BUFFSIZE_TX	0x7844
#define RTL837X_REG_NIC_RXBUFF_RX	0x7848
#define RTL837X_REG_RX_CTRL		0x785c
#define RTL837X_REG_TX_CTRL		0x7860
#define RTL837X_REG_RX_AVAIL		0x7874
#define RTL837X_REG_RX_RINGPTR		0x787c
#define RTL837X_REG_RX_DONE		0x784c
#define RTL837X_REG_CPU_TAG		0x6720
#define RTL837X_REG_CPU_TAG_AWARE_PMASK	0x603C
#define RTL837X_REG_MAC_FORCE_MODE	0x6344

/*
 * Statistics related registers
 */
#define RTL837X_STAT_GET	0x0f60
#define RTL837X_STAT_V_HIGH	0x0f64
#define RTL837X_STAT_V_LOW	0x0f68

/*
 * Table access registers of the RTL837x
 * See e.g. RTL8366/RTL8369 datasheet for explanation
 */
#define RTL837X_TBL_CTRL	0x5cac
/* Bytes in control register: EE EE TT CC: EE: Entry, TT: Table type, CC: Command
 * CC: BIT 0: 01: Execute. Bit 1: 1: WRITE, 0: READ
 * TT: 04: L2-table, 03: VLAN-table
 */
// Table operation bit-smasks
#define TBL_WRITE	0x02
#define TBL_EXECUTE	0x01
// Table types
#define TBL_L2_UNICAST	0x04
#define TBL_VLAN 	0x03

#define RTL837X_L2_CTRL		0x5350
#define L2_CTRL_LUT_IPMC_HASH	3
#define RTL837x_TBL_DATA_0	0x5cb0
#define RTL837x_L2_DATA_OUT_A	0x5ccc
#define RTL837x_L2_DATA_OUT_B	0x5cd0
#define RTL837x_L2_DATA_OUT_C	0x5cd4
#define RTL837x_TBL_DATA_IN_A	0x5cb8
#define RTL837x_TBL_DATA_IN_B	0x5cbc
#define RTL837x_TBL_DATA_IN_C	0x5cc0
#define RTL837x_PVID_BASE_REG	0x4e1c

#define RTL837x_L2_TBL_FLUSH_CTRL	0x53d4
#define L2_TBL_FLUSH_EXEC		0x10000
#define RTL837x_L2_TBL_FLUSH_CNF	0x53dc
#define RTL837X_L2_LRN_PORT_CONSTRAINT	0x5384
#define RTL837X_L2_LRN_PORT_CONSTRT_ACT	0x4f80
#define	RTL8373_REG_MAC_L2_PORT_MAX_LEN	0x1250

/*
 * VLAN configuration
 */
#define RTL837X_VLAN_CTRL		0x4e14
#define VLAN_CVLAN_FILTER		0x4
#define RTL837X_VLAN_PORT_EGR_TAG	0x6738
#define RTL837X_VLAN_PORT_IGR_FLTR	0x4e18
#define RTL837X_VLAN_L2_LRN_DIS_0	0x4e30
#define RTL837X_VLAN_L2_LRN_DIS_1	0x4e34

/*
 * Egress / ingress filtering
 */
// 2 bits per port: allow tagged (01) / untagged (10) and all (00)
#define RTL837x_REG_INGRESS	0x4e10
#define INGR_ALLOW_TAGGED 1
#define INGR_ALLOW_UNTAGGED 2
#define INGR_ALLOW_ALL 0

/*
 * Mirroring
 */
#define RTL837x_MIRROR_CONF		0x604c
#define RTL837x_MIRROR_CTRL		0x6048

/*
 * Link Aggregation aka Trunking
 */
#define RTL837X_TRK_MBR_CTRL_BASE	0x4f38
#define RTL837X_TRK_HASH_CTRL_BASE	0x4f48
#define LAG_HASH_SOURCE_PORT_NUMBER	0x01
#define LAG_HASH_L2_SMAC		0x02
#define LAG_HASH_L2_DMAC		0x04
#define LAG_HASH_L3_SIP			0x08
#define LAG_HASH_L3_DIP			0x10
#define LAG_HASH_L4_SPORT		0x20
#define LAG_HASH_L4_DPORT		0x40
#define LAG_HASH_DEFAULT (LAG_HASH_L2_SMAC | LAG_HASH_L2_DMAC | LAG_HASH_L3_SIP | LAG_HASH_L3_DIP | LAG_HASH_L4_SPORT | LAG_HASH_L4_DPORT)

/*
 * Port isolation
 */
#define RTL837X_PORT_ISOLATION_BASE	0x50c0

/*
 * Multicast handling
 */
#define RTL837X_IPV4_PORT_MC_LM_ACT	0x4f78
#define RTL837X_IPV6_PORT_MC_LM_ACT	0x4f7c
#define RTL837X_IGMP_PORT_CFG		0x52a0
#define IGMP_MAX_GROUP			0x00ff0000
#define IGMP_PROTOCOL_ENABLE		0x00007c00
#define IGMP_TRAP			0x0000002a
#define IGMP_FLOOD			0x00000015
#define IGMP_ASIC			0x00000000
#define RTL837X_IGMP_ROUTER_PORT	0x529c
#define RTL837X_IPV4_UNKN_MC_FLD_PMSK	0x5368
#define RTL837X_IPV6_UNKN_MC_FLD_PMSK	0x536c
#define RTL837X_IGMP_TRAP_CFG		0x50bc
#define IGMP_TRAP_PRIORITY		0x7
#define IGMP_CPU_PORT			0x00010000

/*
 * Loop detection / STP
 */
#define RTL8373_RLDP_TIMER		0x1074
#define RTL837X_RMA0_CONF		0x4ecc
#define RTL837X_RMA_CONF		0x4f1c
#define RTL837X_MSTP_STATES		0x5310
#define RTL837X_REG_LED_RLDP_1		0x65F8
#define RTL837X_REG_LED_RLDP_2		0x65FC
#define RTL837X_REG_LED_RLDP_3		0x65FC

/*
 * EEE
 */
#define RTL837X_EEE_STATUS		0x125C
#define RTL837X_MAC_EEE_ABLTY		0x6404
#define RTL8373_PHY_EEE_ABLTY		0x642C
#define RTL8373_EEE_CTRL_BASE		0x606c
#define EEE_100 	0x01
#define EEE_1000	0x04
#define EEE_2G5		0x10

/*
 * RANDOM
 */
#define RTL837X_RLDP_RLPP		0x106C
#define RLDP_RND_EN			3
#define RTL837X_RAND_NUM0		0x107C
#define RTL837X_RAND_NUM1		0x1080

#ifdef REGDBG

#define REG_SET(r, v) SFR_DATA_24 = (((uint32_t)v) >> 24) & 0xff; \
	SFR_DATA_16 = (((uint32_t)v) >> 16) & 0xff; \
	SFR_DATA_8 = (((uint16_t)v) >> 8 & 0xff); \
	SFR_DATA_0 = (v) & 0xff; \
	reg_write(r); \
	write_char('R'); print_byte(r >> 8); print_byte(r); write_char('-'); \
	print_byte(((v) >> 24) & 0xff); print_byte((v) >> 16 & 0xff); print_byte((v) >> 8 & 0xff); print_byte( (v) & 0xff); write_char(' ');

#define	REG_WRITE(r, v24, v16, v8, v0) SFR_DATA_24 = (v24); \
	SFR_DATA_16 = (v16); \
	SFR_DATA_8 = (v8); \
	SFR_DATA_0 = (v0); \
	reg_write(r); \
	write_char('R'); print_byte(r>>8); print_byte(r); write_char('-'); print_byte(v24); print_byte(v16); print_byte(v8); print_byte(v0); write_char(' ');
#else
#define REG_SET(r, v) SFR_DATA_24 = (((uint32_t)v) >> 24) & 0xff; \
	SFR_DATA_16 = (((uint32_t)v) >> 16) & 0xff; \
	SFR_DATA_8 = (((uint16_t)v) >> 8 & 0xff); \
	SFR_DATA_0 = (v) & 0xff; \
	reg_write(r);

#define	REG_WRITE(r, v24, v16, v8, v0) SFR_DATA_24 = (v24); \
	SFR_DATA_16 = (v16); \
	SFR_DATA_8 = (v8); \
	SFR_DATA_0 = (v0); \
	reg_write(r);
#endif

#endif

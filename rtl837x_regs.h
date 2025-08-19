#ifndef _RTL837X_REGS_H_
#define _RTL837X_REGS_H_

#define RTL837X_REG_HW_CONF 0x6040
// Bits 4 & 5: CLOCK DIVIDER from 125MHz for Timer

#define RTL837X_REG_LED_MODE 0x6520
// Defines the LED Mode for steering the Port LEDS and the System LED
// BIT 17 set: LED solid on
// Bytes 0/1 hold the LED mode, e.g. serial, RTL8231?
// Blink rate is defined by setAsicRegBits(0x6520,0xe00000,rate);

#define RTL837X_REG_SMI_CTRL 0x6454
#define RTL837X_REG_RESET 0x0024
// Writing 0x01 into this register causes a reset of the entire SoC

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

#define RTL837X_REG_LINKS 0x63f0
/* Each nibble encodes the link state of a port.
   Port 0 appears to be the CPU port
   The RTL8372 serves ports 4-7, port 3 is the RTL8221
   2: 1Gbit
   5: 2.5Gbit
  */

#define RTL837X_REG_GPIO_A 0x40
// BIT 4 resets RTL8224 on 9000-9XH

#define RTL837X_REG_GPIO_B 0x44
// Bit 1e cleared: SFP Module inserted on 9000-6XH (MOD_DEF0 pin)

#define RTL837X_REG_GPIO_C 0x48
// BIT 5 set: SIGNAL LOS of SFP module on 9000-6XH (RX_LOS pin)

#define RTL837X_REG_GPIO_CONF_A 0x50
// Configures IO direction for bank a

#define RTL837X_REG_GPIO_EXT 0x63e8

/*
 * I2C controller
 */
#define RTL837X_REG_I2C_CTRL	0x0418
#define RTL837X_REG_I2C_IN	0x0420
#define RTL837X_REG_I2C_OUT	0x0424

/*
 * NIC Related registers
 */
#define RTL837X_REG_RX_CTRL	0x785c
#define RTL837X_REG_TX_CTRL	0x7860
#define RTL837X_REG_RX_AVAIL	0x7874
#define RTL837X_REG_RX_RINGPTR	0x787c
#define RTL837X_REG_RX_DONE	0x784c

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

#define RTL837x_TBL_DATA_0	0x5cb0
#define RTL837x_L2_DATA_OUT_A	0x5ccc
#define RTL837x_L2_DATA_OUT_B	0x5cd0
#define RTL837x_L2_DATA_OUT_C	0x5cd4
#define RTL837x_TBL_DATA_IN_A	0x5cb8
#define RTL837x_L2_TBL_CTRL	0x53d4
#define RTL837x_PVID_BASE_REG	0x4e1c

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
#define RTL837x_MIRROR_CONF 0x604c
#define RTL837x_MIRROR_CTRL 0x6048

/*
 * Trunking
 */
#define RTL837x_TRUNK_CTRL_A	0x4f38
#define RTL837x_TRUNK_CTRL_B	0x4f3c

#ifdef REGDBG

#define REG_SET(r, v) SFR_DATA_24 = ((v) >> 24) & 0xff; \
	SFR_DATA_16 = ((v) >> 16) & 0xff; \
	SFR_DATA_8 = ((v) >> 8 & 0xff); \
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
#define REG_SET(r, v) SFR_DATA_24 = ((v) >> 24) & 0xff; \
	SFR_DATA_16 = ((v) >> 16) & 0xff; \
	SFR_DATA_8 = ((v) >> 8 & 0xff); \
	SFR_DATA_0 = (v) & 0xff; \
	reg_write(r);

#define	REG_WRITE(r, v24, v16, v8, v0) SFR_DATA_24 = (v24); \
	SFR_DATA_16 = (v16); \
	SFR_DATA_8 = (v8); \
	SFR_DATA_0 = (v0); \
	reg_write(r);
#endif

#endif

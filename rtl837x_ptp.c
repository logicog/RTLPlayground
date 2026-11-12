#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_ptp.h"
#include "rtl837x_phy.h"
#include "machine.h"
#include "phy.h"

#pragma codeseg BANK4
#pragma constseg BANK4

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];

__xdata bool ptp_tx_ts_valid;
__xdata uint8_t ptp_tx_ts_port;
__xdata uint8_t ptp_tx_ts_msg_type;
__xdata uint8_t ptp_tx_ts_seq_id[2]; // This is in Network byte order
__xdata uint8_t ptp_tx_ts_sec;
__xdata uint8_t ptp_sec[6]; // 48bit time in seconds (BE)
__xdata uint8_t ptp_nsec[4]; // 30bit of nsec (BE)

void ptp_init(void) __banked
{
	print_string("PTP initializing...\n");
	for (uint8_t i = INT_PHY_MIN_PORT; i <= INT_PHY_MAX_PORT; i++) {
		print_string("PTP init port "); print_byte(i); write_char(' ');
		reg_bit_clear(RTL837X_P0_MISC_CTRL + (i << 5), P0_CFG_BYPASS_BIT);
		// Set Port TX ID 0x00 00 00 01 20 01
		SFR_DATA_24 = 0;
		SFR_DATA_16 = 0;
		SFR_DATA_8 = 0;
		SFR_DATA_0 = machine.log_to_phys_port[i];
		reg_write(RTL837X_P0_PTP_PORTID + (i << 5));
		write_char('\n');

		// Trap different PTP messages to CPU, trap bits are 0b10
		reg_bit_clear(RTL837X_RMA_PORT_PTP_ETH2_CTRL, i << 1);
		reg_bit_set(RTL837X_RMA_PORT_PTP_ETH2_CTRL, (i << 1) + 1);
		reg_bit_clear(RTL837X_RMA_PORT_PTP_UDP_CTRL, i << 1);
		reg_bit_set(RTL837X_RMA_PORT_PTP_UDP_CTRL, (i << 1) + 1);
		reg_bit_clear(RTL837X_RMA_PORT_PTP_DELAY_CARE_CTRL, i);
		reg_bit_set(RTL837X_RMA_PORT_PTP_DELAY_CARE_CTRL, i);
		reg_bit_clear(RTL837X_RMA_PORT_PTP_PDELAY_CARE_CTRL, i);
		reg_bit_set(RTL837X_RMA_PORT_PTP_PDELAY_CARE_CTRL, i);
		reg_bit_clear(RTL837X_RMA_PORT_PTP_ASM_CARE_CTRL, i);
		reg_bit_set(RTL837X_RMA_PORT_PTP_ASM_CARE_CTRL, i);
	}
 
	// Use internal clock
	reg_bit_clear(RTL837X_PTP_CLK_SRC_CTRL, CLK_SOURCE_BIT);
	REG_SET(RTL837X_PTP_TIME_FREQ0, PTP_CLK_FREQ & 0xffff);
	REG_SET(RTL837X_PTP_TIME_FREQ1, (PTP_CLK_FREQ >> 16) & 0xffff);

	reg_bit_set(RTL837X_PTP_APPLY_FREQ, 0);
	for (uint8_t i = 0; i < 5; i++) {
		reg_read(RTL837X_PTP_APPLY_FREQ);
		if (!(SFR_DATA_0 & 0x1))
			break;
	}
	if (SFR_DATA_0 & 0x1)
		print_string("PTP clock setup failed");

	REG_SET(RTL837X_PTP_TIME_OP_CTRL, PTP_TIME_OP_GPI_RISE_TRIG);
	REG_SET(RTL837X_TOD_UART_SETTING, 0x0200);
	REG_SET(RTL837X_TOD_UART_SETTING, 0x0300);
}


void ptp_rtl8224_init(void) __banked
{
	print_string("PTP RTL8224 initializing...\n");

	for (uint8_t i = 0; i < 4; i++) {
		rtl8224_read_reg_u16(RTL837X_P0_MISC_CTRL + (i << 5));
		rtl8224_write_reg_u16(RTL837X_P0_MISC_CTRL + (i << 5),
				      ((uint16_t)SFR_DATA_8 << 8) | (SFR_DATA_0 & 0xfe));
	}

	// Use internal clock
	rtl8224_read_reg_u16(RTL837X_PTP_CLK_SRC_CTRL);
	rtl8224_write_reg_u16(RTL837X_PTP_CLK_SRC_CTRL, ((uint16_t)SFR_DATA_8 << 8) | (SFR_DATA_0 & 0xfe));

	rtl8224_write_reg_u16(RTL837X_PTP_TIME_FREQ0, PTP_CLK_FREQ & 0xffff);
	rtl8224_write_reg_u16(RTL837X_PTP_TIME_FREQ1, (PTP_CLK_FREQ >> 16) & 0xffff);

	rtl8224_write_reg_u16(RTL837X_PTP_APPLY_FREQ, 1);

	rtl8224_write_reg_u16(RTL837X_PTP_TIME_OP_CTRL, 0xc0);
}

#define RTL8221_PTP_CTL 0xe400
#define RTL8221_PTP_CTL2 0xe500
#define RTL8221_AN_CTL 0x0000
#define RTL8221_SYNCE_CTL 0xe406
#define RTL8221_PTP_CLK_CFG 0xe410
#define RTL8221_PTP_CFG_NS_LO 0xe412
#define RTL8221_PTP_CFG_NS_HI 0xe414
#define RTL8221_PTP_CFG_S_LO 0xe416
#define RTL8221_PTP_CFG_S_MI 0xe418
#define RTL8221_PTP_CFG_S_HI 0xe41A

void ptp_rtl8221_init(uint8_t phy) __banked
{
	print_string("PTP RTL8221 initializing...\n");

	// Enable various support for PTP protocols
	phy_modify(phy, PHY_MMD31, RTL8221_PTP_CTL, 0x0000, 0x0fff);
	// Sync-E support ???
//	phy_modify(phy, PHY_MMD31, RTL8221_SYNCE_CTL, 0x0000, 0x0001);
	phy_modify(phy, PHY_MMD31, RTL8221_PTP_CTL2, 0x0000, 0x5234);

	phy_modify(phy, PHY_MMD31, RTL8221_PTP_CLK_CFG, 0x007f, 0x0031);

	// Restart AN to make PTP changes take
	phy_modify(phy, PHY_MMD_AN, RTL8221_AN_CTL, 0x0000, 0x8000);
}

void ptp_read_tx_timestamp(void) __banked
{
	reg_read(RTL837X_PTP_TX_TIMESTAMP_RD0);
	ptp_tx_ts_valid = SFR_DATA_8 & 0x80;
	ptp_tx_ts_port = SFR_DATA_8 & 0x3f;
	ptp_tx_ts_msg_type = SFR_DATA_0 >> 6;
	ptp_tx_ts_seq_id[0] = SFR_DATA_0 << 2;

	reg_read(RTL837X_PTP_TX_TIMESTAMP_RD1);
	ptp_tx_ts_seq_id[0] |= SFR_DATA_8 >> 6;
	ptp_tx_ts_seq_id[1] = SFR_DATA_8 << 2;
	ptp_tx_ts_seq_id[1] |= SFR_DATA_0 >> 6;
	ptp_tx_ts_sec = SFR_DATA_0 << 2;

	reg_read(RTL837X_PTP_TX_TIMESTAMP_RD2);
	ptp_tx_ts_sec |= SFR_DATA_8 >> 6;
}


void ptp_get_time(void) __banked
{
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = 0;
	SFR_DATA_0 = PTP_TIME_READ | PTP_TIME_CMD_EXEC;
	reg_write(RTL837X_PTP_TIME_CRTL);

	for (uint8_t i = 0; i < 20; i++) {
		reg_read(RTL837X_PTP_TIME_CRTL);
		if (!(SFR_DATA_0 & PTP_TIME_CMD_EXEC))
			break;
	}
	if (SFR_DATA_0 & PTP_TIME_CMD_EXEC) {
		print_string("PTP clock get failed");
		return;
	}
	reg_read(RTL837X_PTP_TIME_SEC_RD2);
	ptp_sec[0] = SFR_DATA_8;
	ptp_sec[1] = SFR_DATA_0;
	reg_read(RTL837X_PTP_TIME_SEC_RD1);
	ptp_sec[2] = SFR_DATA_8;
	ptp_sec[3] = SFR_DATA_0;
	reg_read(RTL837X_PTP_TIME_SEC_RD0);
	ptp_sec[4] = SFR_DATA_8;
	ptp_sec[5] = SFR_DATA_0;
	reg_read(RTL837X_PTP_TIME_NSEC_RD1);
	ptp_nsec[0] = SFR_DATA_8;
	ptp_nsec[1] = SFR_DATA_0;
	reg_read(RTL837X_PTP_TIME_NSEC_RD0);
	ptp_nsec[2] = SFR_DATA_8;
	ptp_nsec[3] = SFR_DATA_0;

	print_string("Time: ");
	print_byte(ptp_sec[0]); print_byte(ptp_sec[1]); print_byte(ptp_sec[2]);
	print_byte(ptp_sec[3]); print_byte(ptp_sec[4]); print_byte(ptp_sec[5]);
	write_char('.');
	print_byte(ptp_nsec[0]); print_byte(ptp_nsec[1]); print_byte(ptp_nsec[2]);
	print_byte(ptp_nsec[3]);
}


void ptp_rtl8224_get_time(void) __banked
{
	print_string("RTL8224 PTP clock get");
	rtl8224_write_reg_u16(RTL837X_PTP_TIME_CRTL, PTP_TIME_READ | PTP_TIME_CMD_EXEC);
	for (uint8_t i = 0; i < 20; i++) {
		rtl8224_read_reg_u16(RTL837X_PTP_TIME_CRTL);
		if (!(SFR_DATA_0 & PTP_TIME_CMD_EXEC))
			break;
	}
	if (SFR_DATA_0 & PTP_TIME_CMD_EXEC) {
		print_string("PTP clock get failed");
		return;
	}

	rtl8224_read_reg_u16(RTL837X_PTP_TIME_SEC_RD2);
	ptp_sec[0] = SFR_DATA_8;
	ptp_sec[1] = SFR_DATA_0;
	rtl8224_read_reg_u16(RTL837X_PTP_TIME_SEC_RD1);
	ptp_sec[2] = SFR_DATA_8;
	ptp_sec[3] = SFR_DATA_0;
	rtl8224_read_reg_u16(RTL837X_PTP_TIME_SEC_RD0);
	ptp_sec[4] = SFR_DATA_8;
	ptp_sec[5] = SFR_DATA_0;
	rtl8224_read_reg_u16(RTL837X_PTP_TIME_NSEC_RD1);
	ptp_nsec[0] = SFR_DATA_8;
	ptp_nsec[1] = SFR_DATA_0;
	rtl8224_read_reg_u16(RTL837X_PTP_TIME_NSEC_RD0);
	ptp_nsec[2] = SFR_DATA_8;
	ptp_nsec[3] = SFR_DATA_0;

	print_string("Time: ");
	print_byte(ptp_sec[0]); print_byte(ptp_sec[1]); print_byte(ptp_sec[2]);
	print_byte(ptp_sec[3]); print_byte(ptp_sec[4]); print_byte(ptp_sec[5]);
	write_char('.');
	print_byte(ptp_nsec[0]); print_byte(ptp_nsec[1]); print_byte(ptp_nsec[2]);
	print_byte(ptp_nsec[3]);
}


void ptp_rtl8221_get_time(uint8_t phy) __banked
{
	print_string("RTL8221 PTP clock get");

	// Read clock
	phy_modify(phy, PHY_MMD31, RTL8221_PTP_CLK_CFG, 0x000f, 0x0005);

	phy_read(phy, PHY_MMD31, RTL8221_PTP_CFG_S_HI);
	ptp_sec[0] = SFR_DATA_8;
	ptp_sec[1] = SFR_DATA_0;
	phy_read(phy, PHY_MMD31, RTL8221_PTP_CFG_S_MI);
	ptp_sec[2] = SFR_DATA_8;
	ptp_sec[3] = SFR_DATA_0;
	phy_read(phy, PHY_MMD31, RTL8221_PTP_CFG_S_LO);
	ptp_sec[4] = SFR_DATA_8;
	ptp_sec[5] = SFR_DATA_0;
	phy_read(phy, PHY_MMD31, RTL8221_PTP_CFG_NS_HI);
	ptp_nsec[0] = SFR_DATA_8;
	ptp_nsec[1] = SFR_DATA_0;
	phy_read(phy, PHY_MMD31, RTL8221_PTP_CFG_NS_LO);
	ptp_nsec[2] = SFR_DATA_8;
	ptp_nsec[3] = SFR_DATA_0;

	print_string("Time: ");
	print_byte(ptp_sec[0]); print_byte(ptp_sec[1]); print_byte(ptp_sec[2]);
	print_byte(ptp_sec[3]); print_byte(ptp_sec[4]); print_byte(ptp_sec[5]);
	write_char('.');
	print_byte(ptp_nsec[0]); print_byte(ptp_nsec[1]); print_byte(ptp_nsec[2]);
	print_byte(ptp_nsec[3]);
}


void ptp_get_clkout(void) __banked
{
	reg_read(RTL837X_PTP_CLKOUT_SEC2);
	ptp_sec[0] = SFR_DATA_8;
	ptp_sec[1] = SFR_DATA_0;
	reg_read(RTL837X_PTP_CLKOUT_SEC1);
	ptp_sec[2] = SFR_DATA_8;
	ptp_sec[3] = SFR_DATA_0;
	reg_read(RTL837X_PTP_CLKOUT_SEC0);
	ptp_sec[4] = SFR_DATA_8;
	ptp_sec[5] = SFR_DATA_0;
	reg_read(RTL837X_PTP_CLKOUT_NSEC1);
	ptp_nsec[0] = SFR_DATA_0;
	ptp_nsec[1] = SFR_DATA_8;
	reg_read(RTL837X_PTP_CLKOUT_NSEC0);
	ptp_nsec[2] = SFR_DATA_8;
	ptp_nsec[3] = SFR_DATA_0;

	print_string("OUT Time: ");
	print_byte(ptp_sec[0]); print_byte(ptp_sec[1]); print_byte(ptp_sec[2]);
	print_byte(ptp_sec[3]); print_byte(ptp_sec[4]); print_byte(ptp_sec[5]);
	write_char('.');
	print_byte(ptp_nsec[0]); print_byte(ptp_nsec[1]); print_byte(ptp_nsec[2]);
	print_byte(ptp_nsec[3]);
}


void ptp_set_time(void) __banked
{
	print_string("\nSetting Time");
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = 0x00;
	SFR_DATA_0 = 0x12;
	reg_write(RTL837X_PTP_TIME_SEC2);
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = 0x34;
	SFR_DATA_0 = 0x56;
	reg_write(RTL837X_PTP_TIME_SEC1);
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = 0x78;
	SFR_DATA_0 = 0x9a;
	reg_write(RTL837X_PTP_TIME_SEC0);
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = 0x80;  // TOD Valid
	SFR_DATA_0 = 0x12;
	reg_write(RTL837X_PTP_TIME_NSEC1);
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = 0x34;
	SFR_DATA_0 = 0x56;
	reg_write(RTL837X_PTP_TIME_NSEC0);

	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = 0;
	SFR_DATA_0 = PTP_TIME_WRITE | PTP_TIME_CMD_EXEC;
	reg_write(RTL837X_PTP_TIME_CRTL);

	for (uint8_t i = 0; i < 20; i++) {
		reg_read(RTL837X_PTP_TIME_CRTL);
		if (!(SFR_DATA_0 & PTP_TIME_CMD_EXEC))
			break;
	}
	if (SFR_DATA_0 & PTP_TIME_CMD_EXEC) {
		print_string("PTP clock set failed");
		return;
	}
}


void ptp_rtl8224_set_time(void) __banked
{
	print_string("\nRTL8224 Setting Time");
	rtl8224_write_reg_u16(RTL837X_PTP_TIME_SEC2, 0x0012);
	rtl8224_write_reg_u16(RTL837X_PTP_TIME_SEC1, 0x3456);
	rtl8224_write_reg_u16(RTL837X_PTP_TIME_SEC0, 0x789a);
	rtl8224_write_reg_u16(RTL837X_PTP_TIME_NSEC1, 0x8012);
	rtl8224_write_reg_u16(RTL837X_PTP_TIME_NSEC0, 0x3456);

	rtl8224_write_reg_u16(RTL837X_PTP_TIME_CRTL, PTP_TIME_WRITE | PTP_TIME_CMD_EXEC);

	for (uint8_t i = 0; i < 20; i++) {
		rtl8224_read_reg_u16(RTL837X_PTP_TIME_CRTL);
		if (!(SFR_DATA_0 & PTP_TIME_CMD_EXEC))
			break;
	}
	if (SFR_DATA_0 & PTP_TIME_CMD_EXEC) {
		print_string("RTL8224 PTP clock set failed");
		return;
	}
	rtl8224_read_reg_u16(RTL837X_PTP_TIME_NSEC0);
	print_byte(SFR_DATA_8); print_byte(SFR_DATA_0);
	write_char('\n');
}

void ptp_rtl8221_set_time(uint8_t phy) __banked
{
	print_string("\nRTL8221 Setting Time ");

	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_S_HI, 0x0012);
	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_S_MI, 0x3456);
	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_S_LO, 0x789a);
	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_NS_HI, 0x0012);
	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_NS_LO, 0x3456);

	// Write clock
	phy_modify(phy, PHY_MMD31, RTL8221_PTP_CLK_CFG, 0x000f, 0x0007);
}

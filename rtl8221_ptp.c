#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_phy.h"
#include "machine.h"
#include "rtl8221_ptp.h"
#include "phy.h"

#pragma codeseg BANK4
#pragma constseg BANK4

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];

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

#ifdef RTL8221_PTP

void ptp_rtl8221_init(__code const struct ptp_module *self) __banked
{
	uint8_t phy = self->phy;

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


void ptp_rtl8221_read_time(__code const struct ptp_module *self) __banked
{
	uint8_t phy = self->phy;

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
}


void ptp_rtl8221_get_time(__code const struct ptp_module *self) __banked
{
	uint8_t phy = self->phy;

	print_string("RTL8221 PTP clock ");
	ptp_rtl8221_read_time(self);
	ptp_print_time();
}


void ptp_rtl8221_set_time(__code const struct ptp_module *self) __banked
{
	uint8_t phy = self->phy;

	print_string("\nRTL8221 Setting Time, PHY ");
	print_byte(phy);
	write_char(' ');

	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_S_HI, 0x0012);
	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_S_MI, 0x3456);
	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_S_LO, 0x789a);
	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_NS_HI, 0x0012);
	phy_write(phy, PHY_MMD31, RTL8221_PTP_CFG_NS_LO, 0x3456);

	// Write clock
	phy_modify(phy, PHY_MMD31, RTL8221_PTP_CLK_CFG, 0x000f, 0x0007);
}

#endif

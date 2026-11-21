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

#ifdef RTL8224_PTP

#define UNREFERENCED_PARAMETER(P) ((void)P)

void ptp_rtl8224_init(__code const struct ptp_module *self) __banked
{
	UNREFERENCED_PARAMETER(self);
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

	rtl8224_write_reg_u16(RTL837X_PTP_TIME_OP_CTRL, PTP_TIME_OP_GPI_RISE_TRIG);
}


void ptp_rtl8224_read_time(__code const struct ptp_module *self) __banked
{
	UNREFERENCED_PARAMETER(self);

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
}


void ptp_rtl8224_get_time(__code const struct ptp_module *self) __banked
{
	print_string("RTL8224 PTP clock ");
	ptp_rtl8224_read_time(self);
	ptp_print_time();
}


void ptp_rtl8224_set_time(__code const struct ptp_module *self) __banked
{
	UNREFERENCED_PARAMETER(self);
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
#endif

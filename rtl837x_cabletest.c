#include <stdint.h>
#include <stdbool.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_cabletest.h"
#include "rtl837x_phy.h"
#include "phy.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

#define RTCT_CTRL		0xa422
#define RTCT_CTRL_START		0x00f1
#define RTCT_CTRL_DONE		0x8000
#define RTCT_SRAM_ADDR		0xa436
#define RTCT_SRAM_DATA		0xa438
#define RTCT_SRAM_STATUS	0x8026
#define RTCT_SRAM_LEN		0x8028

#define RTCT_ST_BUSY		0x01
#define RTCT_ST_MISMATCH	0x06
#define RTCT_ST_OPEN		0x08
#define RTCT_ST_SHORT		0x10
#define RTCT_ST_OK		0x20
#define RTCT_ST_DONE		0x40
#define RTCT_ST_CROSS_SHORT	0x80

#define RTCT_LEN_ZERO		150
#define RTCT_LEN_PER_50CM	43

#define PHY_POWER		0xa610
#define PHY_POWER_OFF		0x0800

#define BMCR_PDOWN_1G_FULL	0x0940
#define BMCR_1G_FULL		0x0140
#define BMCR_ANRESTART		0x0200

#define CT_IDLE			0
#define CT_SILENCE		1
#define CT_RUN			2
#define CT_POLL_TICKS		(SYS_TICK_HZ / 20)
#define CT_POLLS		100

extern volatile __xdata uint32_t ticks;
extern __code const struct machine machine;
extern __xdata struct machine_runtime machine_detected;

__xdata uint8_t ct_state = CT_IDLE;
__xdata uint8_t ct_port;
__xdata uint8_t ct_last;
__xdata uint8_t ct_polls;
__xdata uint8_t ct_valid;
__xdata uint8_t ct_have = 0;
__xdata uint8_t ct_pair;
__xdata uint8_t ct_flags;
__xdata uint16_t ct_bmcr;
__xdata uint8_t ct_result[4];
__xdata uint16_t ct_len[4];

void rtl_phy_cable_test_start(uint8_t port) __banked
{
	phy_write(port, PHY_MMD31, PHY_MMD31_FEDCR, BMCR_1G_FULL);
	phy_write(port, PHY_MMD31, RTCT_CTRL, RTCT_CTRL_START);
}

bool rtl_phy_cable_test_get_status(uint8_t port) __banked
{
	phy_read(port, PHY_MMD31, RTCT_CTRL);
	if (!(SFR_DATA_U16 & RTCT_CTRL_DONE))
		return false;

	for (ct_pair = 0; ct_pair < 4; ct_pair++) {
		// The flags are the low byte of the word, not the high one.
		phy_write(port, PHY_MMD31, RTCT_SRAM_ADDR, RTCT_SRAM_STATUS + (ct_pair << 2));
		phy_read(port, PHY_MMD31, RTCT_SRAM_DATA);
		ct_flags = SFR_DATA_U16;
		if (!(ct_flags & RTCT_ST_DONE))
			return false;
		if (ct_flags & RTCT_ST_OK)
			ct_result[ct_pair] = CABLE_RESULT_OK;
		else if (ct_flags & RTCT_ST_OPEN)
			ct_result[ct_pair] = CABLE_RESULT_OPEN;
		else if (ct_flags & RTCT_ST_SHORT)
			ct_result[ct_pair] = CABLE_RESULT_SAME_SHORT;
		else if (ct_flags & RTCT_ST_BUSY)
			ct_result[ct_pair] = CABLE_RESULT_UNSPEC;
		else if (ct_flags & RTCT_ST_CROSS_SHORT)
			ct_result[ct_pair] = CABLE_RESULT_CROSS_SHORT;
		else if (ct_flags & RTCT_ST_MISMATCH)
			ct_result[ct_pair] = CABLE_RESULT_MISMATCH;
		else
			ct_result[ct_pair] = CABLE_RESULT_UNSPEC;

		phy_write(port, PHY_MMD31, RTCT_SRAM_ADDR, RTCT_SRAM_LEN + (ct_pair << 2));
		phy_read(port, PHY_MMD31, RTCT_SRAM_DATA);
		ct_len[ct_pair] = SFR_DATA_U16;
		ct_len[ct_pair] = ct_len[ct_pair] > RTCT_LEN_ZERO ? ct_len[ct_pair] - RTCT_LEN_ZERO : 0;
		ct_len[ct_pair] = ct_len[ct_pair] * 5 / RTCT_LEN_PER_50CM;
	}
	return true;
}

static void cabletest_print(void)
{
	print_string("Cable test port ");
	print_phys_port(ct_port);
	if (!ct_valid) {
		print_string(": timed out\n");
		return;
	}
	write_char('\n');
	for (ct_pair = 0; ct_pair < 4; ct_pair++) {
		print_string("Pair ");
		write_char('A' + ct_pair);
		switch (ct_result[ct_pair]) {
		case CABLE_RESULT_OK:
			print_string(": ok\n");
			continue;
		case CABLE_RESULT_OPEN:
			print_string(": open at ");
			break;
		case CABLE_RESULT_SAME_SHORT:
			print_string(": short at ");
			break;
		case CABLE_RESULT_CROSS_SHORT:
			print_string(": short to another pair at ");
			break;
		case CABLE_RESULT_MISMATCH:
			print_string(": impedance mismatch at ");
			break;
		default:
			print_string(": unknown\n");
			continue;
		}
		itoa_short(ct_len[ct_pair] / 10);
		write_char('.');
		write_char('0' + ct_len[ct_pair] % 10);
		print_string(" m\n");
	}
}

void cabletest_run(void) __banked
{
	if (ct_state) {
		print_string("Cable test running on port ");
		print_phys_port(ct_port);
		write_char('\n');
		return;
	}
	if (!machine_detected.isRTL8373 && (phy_settings.port < 4 || phy_settings.port > 7)) {
		print_string("No cable test on this PHY\n");
		return;
	}

	// A test started on a port that is off never ends and blocks the PHY until reset.
	phy_read(phy_settings.port, PHY_MMD31, PHY_POWER);
	if (SFR_DATA_U16 & PHY_POWER_OFF) {
		print_string("Port is off\n");
		return;
	}

	ct_port = phy_settings.port;
	phy_read(ct_port, PHY_MMD31, PHY_MMD31_FEDCR);
	ct_bmcr = SFR_DATA_U16;

	// Forcing the speed leaves the link up; only a power-down silences the partner.
	phy_write(ct_port, PHY_MMD31, PHY_MMD31_FEDCR, BMCR_PDOWN_1G_FULL);
	ct_last = ticks;
	ct_have = 0;
	ct_state = CT_SILENCE;
	print_string("Cable test started\n");
}

void cabletest_show(void) __banked
{
	if (ct_state)
		print_string("Cable test running\n");
	else if (ct_port != phy_settings.port || !ct_have)
		print_string("No cable test result\n");
	else
		cabletest_print();
}

void cabletest_tick(void) __banked
{
	if (ct_state == CT_SILENCE) {
		if (!TICKS_DUE(ct_last, SYS_TICK_HZ))
			return;
		phy_read(ct_port, PHY_MMD31, PHY_POWER);
		if (SFR_DATA_U16 & PHY_POWER_OFF) {
			phy_write(ct_port, PHY_MMD31, PHY_MMD31_FEDCR, ct_bmcr);
			ct_state = CT_IDLE;
			print_string("Cable test aborted, port is off\n");
			return;
		}
		rtl_phy_cable_test_start(ct_port);
		ct_polls = CT_POLLS;
		ct_last = ticks;
		ct_state = CT_RUN;
		return;
	}
	if (ct_state != CT_RUN || !TICKS_DUE(ct_last, CT_POLL_TICKS))
		return;
	ct_last = ticks;

	ct_valid = rtl_phy_cable_test_get_status(ct_port);
	if (!ct_valid && --ct_polls)
		return;

	phy_write(ct_port, PHY_MMD31, RTCT_CTRL, 0);
	phy_write(ct_port, PHY_MMD31, PHY_MMD31_FEDCR, ct_bmcr | BMCR_ANRESTART);
	ct_have = 1;
	ct_state = CT_IDLE;
	cabletest_print();
}

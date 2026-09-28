/*
 * test_rma.c - rtl837x_rma.c against the simulated register file.
 *
 * Each rma command is parsed by the firmware and the register it writes is
 * compared with the value built from the field layout: action in bits 5:4,
 * storm control disabled in bit 3, keep tag 2, VLAN leaky 1, isolation leaky 0.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_rma.h"
#include "cmd_parser.h"
#include "support.h"
#include "hw_mock.h"

uint8_t cmd_words_len;
uint8_t cmd_words_b[15];
extern bool stp_enabled;

static void run(const char *line)
{
	memset(cmd_buffer, 0, CMD_BUF_SIZE);
	strcpy((char *)cmd_buffer, line);
	cmd_words_len = 0;
	for (uint8_t i = 0; cmd_buffer[i]; i++)
		if (cmd_buffer[i] != ' ' && (!i || cmd_buffer[i - 1] == ' '))
			cmd_words_b[cmd_words_len++] = i;
	err_status = 0;
	out_reset();
	rma_cmd();
}

static uint32_t op(uint8_t n)
{
	return hw_reg_get(RTL837X_RMA0_CONF + 4 * n);
}

static void t_entries(void)
{
	printf("[test] rma entries\n");
	hw_reset();
	stp_enabled = 0;

	run("rma 02 trap");
	CHECK(op(2) == 0x10 && !err_status, "rma 02 trap sets the action to 1");
	run("rma 02 storm off keep on");
	CHECK(op(2) == 0x1c, "storm off sets bit 3, keep sets bit 2, the action stays");
	run("rma 02 vlanleak on isoleak on storm on");
	CHECK(op(2) == 0x17, "vlanleak and isoleak are bits 1 and 0, storm on clears bit 3");
	run("rma lldp drop");
	CHECK(op(19) == 0x20, "lldp is the 20th register, 0x4f18");
	run("rma 0e nocpu");
	CHECK(hw_reg_get(0x4ee8) == 0x30, "0e is at 0x4ee8 and nocpu is action 3");
	run("rma 20 trap");
	CHECK(hw_reg_get(0x4f04) == 0x10, "20 is at 0x4f04");
	run("rma cdp trap");
	CHECK(hw_reg_get(0x4f10) == 0x10, "cdp is at 0x4f10");

	hw_reg_set(RTL837X_RMA0_CONF + 4 * 3, 0x10);
	run("rma 03 drop keep maybe");
	CHECK(err_status && op(3) == 0x10, "a bad word leaves the register untouched");
	run("rma 05 trap");
	CHECK(err_status, "an address without a register is refused");
	run("rma 03");
	CHECK(err_status, "an entry without settings is refused");

	stp_enabled = 1;
	hw_reg_set(RTL837X_RMA0_CONF, 0x10);
	run("rma 00 forward");
	CHECK(err_status && op(0) == 0x10 && strstr(out_buf, "STP"),
	      "the action of 00 is refused while STP runs");
	run("rma 00 storm off");
	CHECK(!err_status && op(0) == 0x18, "the flags of 00 can still be set while STP runs");
	stp_enabled = 0;
}

static void t_globals(void)
{
	printf("[test] rma globals and PTP\n");
	hw_reset();
	hw_reg_set(RTL837X_RMA_CONF, 0x8);
	run("rma priority 6");
	CHECK(hw_reg_get(RTL837X_RMA_CONF) == 0xe, "priority is bits 2:0 of RMA_CFG and keeps lldpmatch");
	run("rma lldpmatch off");
	CHECK(hw_reg_get(RTL837X_RMA_CONF) == 0x6, "lldpmatch is bit 3");
	run("rma priority 8");
	CHECK(err_status && hw_reg_get(RTL837X_RMA_CONF) == 0x6, "priority above 7 is refused");

	run("rma ptp priority 5");
	run("rma ptp cpu 2");
	CHECK(hw_reg_get(RTL837X_RMA_PTP_TRAP_CTRL) == 0x20005, "ptp priority bits 2:0, cpu bits 17:16");

	run("rma ptp 9 eth2 trap udp drop pdelay on");
	CHECK(hw_reg_get(RTL837X_RMA_PTP_ETH2_CTRL) == 1u << 16 && hw_reg_get(RTL837X_RMA_PTP_UDP_CTRL) == 2u << 16
	      && hw_reg_get(RTL837X_RMA_PTP_PDELAY_CARE) == 1u << 8, "port 9 is ASIC port 8: two bits at 17:16, one at bit 8");
	run("rma ptp 2 eth2 nocpu asm on delay on");
	CHECK(hw_reg_get(RTL837X_RMA_PTP_ETH2_CTRL) == ((1u << 16) | (3u << 2))
	      && hw_reg_get(RTL837X_RMA_PTP_ASM_CARE) == 2 && hw_reg_get(RTL837X_RMA_PTP_DELAY_CARE) == 2,
	      "port 2 is ASIC port 1, other ports stay");
	run("rma ptp 2 udp trap bogus on");
	CHECK(err_status && hw_reg_get(RTL837X_RMA_PTP_UDP_CTRL) == 2u << 16, "a bad PTP line changes nothing");
}

static void t_show(void)
{
	printf("[test] rma show\n");
	hw_reset();
	hw_reg_set(RTL837X_RMA0_CONF + 4, 0x20);
	hw_reg_set(RTL837X_RMA0_CONF + 4 * 19, 0x1b);
	hw_reg_set(RTL837X_RMA_CONF, 0xd);
	hw_reg_set(RTL837X_RMA_PTP_UDP_CTRL, 1u << 4);
	run("rma show");
	CHECK(strstr(out_buf, "rma 01 drop storm on keep off vlanleak off isoleak off\n") != NULL,
	      "show prints an entry as the command that sets it");
	CHECK(strstr(out_buf, "rma lldp trap storm off keep off vlanleak on isoleak on\n") != NULL,
	      "show inverts bit 3 into storm off");
	CHECK(strstr(out_buf, "rma priority 5\nrma lldpmatch on\n") != NULL, "show prints RMA_CFG");
	CHECK(strstr(out_buf, "rma ptp 3 eth2 forward udp trap delay off pdelay off asm off\n") != NULL,
	      "show prints PTP per front port");

	int lines = 0;
	for (const char *p = out_buf; (p = strstr(p, "rma ")); p++) {
		const char *e = strchr(p, '\n');
		if (!e || strncmp(p, "rma ptp ", 8) == 0 || strncmp(p, "rma priority", 12) == 0 || strncmp(p, "rma lldpmatch", 13) == 0)
			continue;
		lines++;
	}
	CHECK(lines == RMA_ENTRIES, "show lists every entry");

	char saved[8192];
	snprintf(saved, sizeof(saved), "%s", out_buf);
	hw_reset();
	for (char *p = saved, *e; (e = strchr(p, '\n')); p = e + 1) {
		*e = 0;
		run(p);
	}
	CHECK(op(1) == 0x20 && op(19) == 0x1b && hw_reg_get(RTL837X_RMA_CONF) == 0xd
	      && hw_reg_get(RTL837X_RMA_PTP_UDP_CTRL) == 1u << 4, "replaying show restores every register");
}

int main(void)
{
	printf("== rtl837x_rma.c tests ==\n");
	t_entries();
	t_globals();
	t_show();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

/*
 * test_acl.c - rtl837x_acl.c and rtl837x_acl_cmd.c against the simulated table engine.
 *
 * Each acl command is parsed by the firmware and the rule and action entries it
 * writes are compared with words built independently from the measured encoding:
 * care half Y = value for a cared bit, X = its complement, both 1 for don't care.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_acl.h"
#include "cmd_parser.h"
#include "support.h"
#include "hw_mock.h"

uint8_t cmd_words_len;
uint8_t cmd_words_b[15];

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
	acl_cmd();
}

struct expect {
	const char *cmd;
	uint8_t     idx;
	uint32_t    x[5], y[5], act[3];
	uint16_t    ctrl;
};

static const struct expect rules[] = {
	{"acl 1 dmac b8:27:eb:4d:27:e9 drop 4", 0, {0x14b2d816, 0xffff47d8, 0xffffffff, 0xffffffff, 0xffffffff}, {0xeb4d27e9, 0xffffb827, 0xffffffff, 0xffffffff, 0xffe047f8}, {0x00000000, 0x00020000, 0x00000000}, 0x20},
	{"acl 2 sip 10.9.0.1 drop 4", 1, {0xf5f6fffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xfffffffe}, {0x0a090001, 0xffffffff, 0xffffffff, 0xffffffff, 0xffe047f9}, {0x00000000, 0x00020000, 0x00000000}, 0x20},
	{"acl 3 udp dport 5678 drop", 2, {0xffffffff, 0xffffffff, 0xffffffff, 0xffffe9d1, 0xfffffbfe}, {0xffffffff, 0xffffffff, 0xffffffff, 0xffff162e, 0xffeffcf9}, {0x00000000, 0x00020000, 0x00000000}, 0x20},
	{"acl 4 vlan 2 pri 5 permit", 3, {0xffffffff, 0xffffffff, 0xffffffff, 0x5ffdffff, 0xffffffff}, {0xffffffff, 0xffffffff, 0xffffffff, 0xb002ffff, 0xffeffff8}, {0x00000000, 0x00000000, 0x00000000}, 0x20},
	{"acl 5 dport 1000-2000 drop", 4, {0xffffffff, 0xfffffffe, 0xffffffff, 0xffffffff, 0xfffffffd}, {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffeffffa}, {0x00000000, 0x00020000, 0x00000000}, 0x20},
	{"acl 6 sip 10.9.0.0/24 dport 1000-2000 drop", 5, {0xfffeffff, 0xfffffffd, 0xffffffff, 0xffffffff, 0xfffffffd}, {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffeffffa}, {0x00000000, 0x00020000, 0x00000000}, 0x20},
	{"acl 7 and ethertype 0806", 6, {0xffffffff, 0xffffffff, 0xffffffff, 0xfffff7f9, 0xffffffff}, {0xffffffff, 0xffffffff, 0xffffffff, 0xffff0806, 0xffeffff8}, {0x00000000, 0x00000000, 0x00000000}, 0x0},
	{"acl 8 not ethertype 88b5 dscp 46 count 5 3 4", 7, {0xffffffff, 0xffffffff, 0xffffffff, 0xffff774a, 0xffffffff}, {0xffffffff, 0xffffffff, 0xffffffff, 0xffff88b5, 0xffe067f8}, {0x00000000, 0x000116e8, 0x00000000}, 0x118},
	{"acl 9 dmac 02:00:00:00:00:00/ff:ff:ff:ff:ff:00 outvlan 40 tag mirror 5", 8, {0xffffffff, 0xfffffdff, 0xffffffff, 0xffffffff, 0xffffffff}, {0x000000ff, 0xffff0200, 0xffffffff, 0xffffffff, 0xffeffff8}, {0x00010285, 0x02040000, 0x00000000}, 0x21},
	{"acl 10 field 3 4143 isolate 3,9 bypass stp interrupt", 9, {0xffffffff, 0xffffffff, 0xffffffff, 0xbebcffff, 0xfffffffd}, {0xffffffff, 0xffffffff, 0xffffffff, 0x4143ffff, 0xffeffffa}, {0x00000000, 0xa0900000, 0x00000040}, 0xe0},
	{"acl 11 svlan 100 setvlan 7 untag", 10, {0xffffffff, 0xffffffff, 0xffffff9b, 0xffffffff, 0xfffffffb}, {0xffffffff, 0xffffffff, 0xfffff064, 0xffffffff, 0xffeffffc}, {0x00000074, 0x00000000, 0x00000000}, 0x1},
	{"acl 12 tcp sport 80 trap", 11, {0xffffffff, 0xffffffff, 0xffafffff, 0xffffffff, 0xfffffcfe}, {0xffffffff, 0xffffffff, 0x0050ffff, 0xffffffff, 0xffeffbf9}, {0x00000000, 0x00060000, 0x00000000}, 0x20},
	{"acl 13 sip6 00000001 police 7 1", 12, {0xfffdffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xfffffffd}, {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffe00ffa}, {0x00000000, 0x00001c00, 0x00000000}, 0x10},
};

static void t_rules(void)
{
	printf("[test] acl rules encode as measured on the switch\n");
	hw_reset();
	for (unsigned n = 0; n < sizeof(rules) / sizeof(rules[0]); n++) {
		const struct expect *e = &rules[n];
		char msg[160];
		int ok = 1;
		run(e->cmd);
		for (int w = 0; w < 5; w++)
			ok &= hw_acl_rule(e->idx, w) == e->x[w] && hw_acl_rule(0x80 | e->idx, w) == e->y[w];
		for (int w = 0; w < 3; w++)
			ok &= hw_acl_act(e->idx, w) == e->act[w];
		ok &= hw_reg_get(RTL837X_ACL_ACT_CTRL + 4 * e->idx) == e->ctrl;
		ok &= err_status == 0;
		snprintf(msg, sizeof(msg), "%s", e->cmd);
		CHECK(ok, msg);
		if (!ok) {
			for (int w = 0; w < 5; w++)
				printf("    w%d X %08x/%08x Y %08x/%08x\n", w, hw_acl_rule(e->idx, w), e->x[w],
				       hw_acl_rule(0x80 | e->idx, w), e->y[w]);
			printf("    act %08x %08x %08x ctrl %x err %d out %s\n", hw_acl_act(e->idx, 0),
			       hw_acl_act(e->idx, 1), hw_acl_act(e->idx, 2),
			       hw_reg_get(RTL837X_ACL_ACT_CTRL + 4 * e->idx), err_status, out_buf);
		}
	}
	CHECK(hw_reg_get(RTL837X_ACL_TEMPLATE0_F0_3) == 0x03020100
	      && hw_reg_get(RTL837X_ACL_TEMPLATE0_F4_7) == 0x08060504, "template 0 = dmac, smac, ethertype, ctag");
	CHECK(hw_reg_get(RTL837X_ACL_TEMPLATE0_F0_3 + 8) == 0x13121110
	      && hw_reg_get(RTL837X_ACL_TEMPLATE0_F4_7 + 8) == 0x08363534, "template 1 = sip, dip, tos/proto, l4 ports, ctag");
	CHECK(hw_reg_get(RTL837X_ACL_PORT_EN) == 0x1ff && hw_reg_get(RTL837X_ACL_UNMATCH_PERMIT) == 0x1ff,
	      "ACL on every front port, unmatched frames permitted");
	CHECK(hw_reg_get(RTL837X_ACL_RNG_PORT) == 2 && hw_reg_get(RTL837X_ACL_RNG_PORT + 4) == (2000u << 16 | 1000),
	      "port range 0 = dport 1000-2000");
	CHECK(hw_reg_get(RTL837X_ACL_RNG_IP) == 1 && hw_reg_get(RTL837X_ACL_RNG_IP + 4) == 0x0a0900ff
	      && hw_reg_get(RTL837X_ACL_RNG_IP + 8) == 0x0a090000, "ip range 0 = sip 10.9.0.0-10.9.0.255 for a prefix");
	CHECK(hw_reg_get(RTL837X_ACL_RNG_IP + 12) == 3 && hw_reg_get(RTL837X_ACL_RNG_IP + 16) == 1,
	      "ip range 1 = sip6 low word 1");

	run("acl 6 off");
	CHECK(hw_reg_get(RTL837X_ACL_RNG_IP) == 0 && hw_reg_get(RTL837X_ACL_RNG_PORT + 8) == 0
	      && hw_reg_get(RTL837X_ACL_RNG_PORT) == 2, "removing a rule frees its ranges and only its ranges");
	CHECK(hw_acl_rule(5, 4) == 0 && hw_acl_rule(0x85, 4) == 0 && hw_acl_act(5, 1) == 0
	      && hw_reg_get(RTL837X_ACL_ACT_CTRL + 20) == 0, "removing a rule clears rule, action and ACT_CTRL");
	run("acl 14 vlan 20-30 drop");
	CHECK(hw_reg_get(RTL837X_ACL_RNG_VID) == (30u << 14 | 20u << 2 | 1), "vid range = lower << 2, upper << 14, cvid");
}

static void t_errors(void)
{
	printf("[test] acl rejects what it cannot express\n");
	hw_reset();
	run("acl 1 dmac 00:11:22:33:44:55 sip 1.2.3.4 drop");
	CHECK(err_status && strstr(out_buf, "and"), "fields without a common template point at acl <n+1> and");
	CHECK(hw_acl_rule(0, 4) == 0, "and nothing is written");
	run("acl 1 dmac 00:11:22:33:44:55");
	CHECK(err_status, "a rule needs an action");
	run("acl 1 not ethertype 0800");
	CHECK(err_status, "not alone is no action");
	run("acl 1 drop permit");
	CHECK(err_status, "one forwarding action per rule");
	run("acl 1 pcp 3 dscp 10");
	CHECK(err_status, "one remark per rule");
	run("acl 65 drop");
	CHECK(err_status, "rule numbers stop at 64");
	run("acl 1 sip 10.1.1.1/33 drop");
	CHECK(err_status, "prefix length stops at 32");
	run("acl 1 ethertype 0800 drop 3 4");
	CHECK(!err_status && hw_acl_rule(0x80, 4) == 0xffe067f8, "trailing ports restrict the rule");
}

static void t_globals(void)
{
	printf("[test] acl meter, field, default, counter\n");
	hw_reset();
	for (int n = 1; n <= ACL_RULES; n++) {
		char line[16];
		snprintf(line, sizeof(line), "acl %d off", n);
		run(line);
	}
	run("acl meter 33 1000 pps 50");
	CHECK(!err_status && hw_reg_get(RTL837X_METER_RATE + 4 * 33) == 1000
	      && hw_reg_get(RTL837X_METER_BURST + 4 * 33) == 50
	      && hw_reg_get(RTL837X_METER_MODE + 4) == 2, "meter 33: rate, burst, pps bit 1 of the second mode word");
	run("acl meter 33 64 kbps 1600");
	CHECK(hw_reg_get(RTL837X_METER_MODE + 4) == 0, "kbps clears the mode bit");
	run("acl field 3 ipv6 8");
	CHECK(hw_reg_get(RTL837X_ACL_FIELD_SEL + 12) == (8 << 3 | 5), "selector 3 = ipv6 at 8");
	run("acl default drop 5 6");
	CHECK(hw_reg_get(RTL837X_ACL_PORT_EN) == 0x1ff && hw_reg_get(RTL837X_ACL_UNMATCH_PERMIT) == 0x1cf,
	      "default drop enables ACL and clears the permit bits of those ports");
	run("acl default permit");
	CHECK(hw_reg_get(RTL837X_ACL_PORT_EN) == 0 && hw_reg_get(RTL837X_ACL_UNMATCH_PERMIT) == 0,
	      "default permit without rules turns ACL off");
	run("acl counter 5 bytes");
	CHECK(hw_reg_get(RTL837X_ACL_LOG_TYPE) == 4, "counter 5 counts bytes through the type bit of its pair");
	run("acl counter reset");
	CHECK(!err_status, "counter reset");
}

int main(void)
{
	printf("== rtl837x_acl.c rule tests ==\n");
	t_rules();
	t_errors();
	t_globals();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

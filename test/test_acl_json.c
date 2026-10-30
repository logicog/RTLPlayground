/* Exercise the ACL read API against rules configured by the production CLI. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_acl.h"
#include "cmd_parser.h"
#include "page_impl.h"
#include "support.h"
#include "hw_mock.h"

uint8_t cmd_words_len;
uint8_t cmd_words_b[15];
extern uint8_t outbuf[TCP_OUTBUF_SIZE];
extern uint16_t slen;

static void run(const char *line)
{
	memset(cmd_buffer, 0, CMD_BUF_SIZE);
	strcpy((char *)cmd_buffer, line);
	cmd_words_len = 0;
	for (uint8_t i = 0; cmd_buffer[i]; i++)
		if (cmd_buffer[i] != ' ' && (!i || cmd_buffer[i - 1] == ' '))
			cmd_words_b[cmd_words_len++] = i;
	err_status = 0;
	acl_cmd();
	CHECK(!err_status, line);
}

static const char *body(void)
{
	CHECK(slen < TCP_OUTBUF_SIZE, "JSON response fits the shared transmit buffer");
	outbuf[slen < TCP_OUTBUF_SIZE ? slen : TCP_OUTBUF_SIZE - 1] = 0;
	const char *p = strstr((char *)outbuf, "\r\n\r\n");
	CHECK(p != NULL, "JSON response has HTTP framing");
	return p ? p + 4 : (char *)outbuf;
}

static unsigned count(const char *s, const char *needle)
{
	unsigned n = 0;
	while ((s = strstr(s, needle))) { n++; s += strlen(needle); }
	return n;
}

static void reset(void)
{
	for (uint8_t i = 0; i < ACL_RULES; i++) acl_rule_clear(i);
	acl_unmatch_drop = 0;
	hw_reset();
	out_reset();
}

static void t_rules(void)
{
	reset();
	run("acl 1 dmac b8:27:eb:4d:27:e9 drop 4");
	run("acl 2 udp dport 5678 drop");
	run("acl 3 and ethertype 0806");
	run("acl 64 not vlan 20 count 5 1");
	/* The web read must not borrow and overwrite CLI staging globals. */
	acl_kv[0] = 0x1234; acl_km[0] = 0x5678;
	acl_ctrl = 0x1357; acl_act.vlan = 0x12345678;
	acl_ra = 0x4321; acl_rv = 0x89abcdef;
	unsigned long reads = hw_reads;
	send_acl_rules(1);
	CHECK(hw_reads - reads == 80, "four rules need no template register reads");
	const char *s = body();
	CHECK(count(s, "\"n\":") == 4 && strstr(s, "\"next\":0}"), "sparse rule page reaches rule 64 without truncation");
	CHECK(strstr(s, "{\"n\":1,\"t\":0,\"ctrl\":32,\"info\":[18424,65504,47111,31]"), "raw Y ingress bits survive the TCAM decode");
	CHECK(strstr(s, "\"fields\":[[10217,65535],[60237,65535],[47143,65535],[0,0]"), "MAC fields decode to normalized value and care bits");
	CHECK(strstr(s, "\"act\":[\"00000000\",\"00020000\",\"00000000\"]"), "action words preserve hardware order and leading zeroes");
	CHECK(strstr(s, "{\"n\":3,\"t\":0,\"ctrl\":0"), "AND continuation remains distinguishable");
	CHECK(strstr(s, "{\"n\":64,"), "last supported rule is included");
	CHECK(acl_kv[0] == 0x1234 && acl_km[0] == 0x5678 && acl_ctrl == 0x1357
		&& acl_act.vlan == 0x12345678 && acl_ra == 0x4321 && acl_rv == 0x89abcdef,
		"reading rules preserves the parser's staged configuration");
	/* Template selection is firmware-owned; read its shadow rather than the
	 * raw TCAM information bits or the template registers. */
	acl_rule_tmpl[0] = 4;
	hw_reg_set(RTL837X_ACL_TEMPLATE0_F0_3, 0xffffffff);
	hw_reg_set(RTL837X_ACL_TEMPLATE0_F4_7, 0xffffffff);
	send_acl_rules(1);
	s = body();
	CHECK(strstr(s, "{\"n\":1,\"t\":4,\"ctrl\":32,\"info\":[18424,"),
		"template index comes from the firmware shadow, independent of hardware template bits");
	acl_rule_tmpl[0] = 0;
	send_acl_rules(64);
	s = body();
	CHECK(count(s, "\"n\":") == 1 && strstr(s, "\"next\":0}"), "starting at rule 64 is bounded");
}

static void t_full_table(void)
{
	reset();
	for (unsigned i = 1; i <= ACL_RULES; i++) {
		char line[100];
		snprintf(line, sizeof(line), "acl %u dmac b8:27:eb:4d:27:e9 smac 00:11:22:33:44:55 ethertype ffff drop", i);
		run(line);
	}
	unsigned next = 1, n = 0, pages = 0;
	do {
		send_acl_rules(next);
		const char *s = body();
		unsigned seen = count(s, "\"n\":");
		CHECK(seen == 4, "full table page contains four complete rules");
		n += seen; pages++;
		const char *p = strstr(s, "\"next\":");
		CHECK(p != NULL, "rule page carries a continuation index");
		next = p ? strtoul(p + 7, NULL, 10) : 0;
	} while (next && pages < 20);
	CHECK(n == ACL_RULES && pages == 16 && next == 0, "pagination visits all 64 rules exactly once");
	reset();
	send_acl_rules(1);
	CHECK(!strcmp(body(), "{\"rules\":[],\"next\":0}"), "empty ACL table produces valid empty JSON");
}

static void t_settings(void)
{
	reset();
	run("acl default drop 5 6");
	run("acl gpio 2 on"); run("acl gpio polarity high");
	run("acl counter 5 mode bytes"); run("acl counter 5 width 64");
	hw_reg_set(RTL837X_ACL_HIT, 0x80000001);
	hw_reg_set(RTL837X_ACL_HIT + 4, 0xffffffff);
	hw_reg_set(RTL837X_METER_EXCEED, 0x80000000);
	hw_reg_set(RTL837X_METER_EXCEED + 4, 1);
	send_acl();
	CHECK(!strcmp(body(), "{\"drop\":48,\"gpio\":4,\"polarity\":1,\"counterMode\":4,\"counterWidth\":4,\"hit\":[\"80000001\",\"ffffffff\"],\"exceeded\":[\"80000000\",\"00000001\"]}"), "global JSON reports CLI configuration and full unsigned status words");
	run("acl field 3 ipv6 255");
	send_acl_fields();
	const char *s = body();
	CHECK(count(s, "\"format\":") == 16 && strstr(s, "{\"format\":5,\"offset\":255}"), "all custom field selectors report format and offset");
	hw_reg_set(RTL837X_ACL_LOG_DATA + 4 * 31, 0xffffffff);
	send_acl_counters();
	s = body();
	CHECK(count(s, "\"00000000\"") == 31 && strstr(s, "\"ffffffff\"]"), "counter API retains all 32 unsigned words");
	run("acl meter 31 16777215 pps 268435455 ifg");
	run("acl meter 32 1234 kbps 5678");
	run("acl meter 63 1 pps 2 ifg");
	send_acl_meters(31);
	s = body();
	CHECK(strstr(s, "{\"n\":31,\"rate\":16777215,\"burst\":268435455,\"pps\":1,\"ifg\":1}"), "meter rates and bursts retain their maximum values");
	CHECK(strstr(s, "{\"n\":32,\"rate\":1234,\"burst\":5678,\"pps\":0,\"ifg\":0}") && strstr(s, "\"next\":39}"), "meter paging crosses the 32-bit mode boundary");
	send_acl_meters(63);
	CHECK(!strcmp(body(), "{\"meters\":[{\"n\":63,\"rate\":1,\"burst\":2,\"pps\":1,\"ifg\":1}],\"next\":0}"), "last meter is paged without reading past the table");
	hw_reg_set(RTL837X_ACL_LOG_DATA, 0x1234);
	out_reset();
	run("acl show");
	CHECK(strstr(out_buf, "0=00001234") && strstr(out_buf, "31=ffffffff")
		&& strstr(out_buf, "hit: 80000001 ffffffff"),
		"CLI register formatting preserves leading zeroes and full unsigned words");
}

static void t_ranges(void)
{
	reset();
	run("acl 1 vlan 100-200 sip 10.0.0.0-10.0.0.255 dport 1000-2000 drop");
	send_acl_ranges(0);
	const char *s = body();
	CHECK(count(s, "\"type\":") == 16 && strstr(s, "{\"type\":1,\"lo\":\"00000064\",\"hi\":\"000000c8\"}"), "VLAN range bit packing is decoded independently");
	send_acl_ranges(1);
	s = body();
	CHECK(count(s, "\"type\":") == 16 && strstr(s, "{\"type\":1,\"lo\":\"0a000000\",\"hi\":\"0a0000ff\"}"), "IP range lower and upper words retain network address order");
	send_acl_ranges(2);
	s = body();
	CHECK(count(s, "\"type\":") == 16 && strstr(s, "{\"type\":2,\"lo\":\"000003e8\",\"hi\":\"000007d0\"}"), "port range halves are decoded in lower/upper order");
}

int main(void)
{
	t_rules(); t_full_table(); t_settings(); t_ranges();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

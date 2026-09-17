/*
 * test_page_json.c - the JSON generators of httpd/page_impl.c, fed by the
 * table code of rtl837x_port.c over the simulated engine. Both modules are
 * compiled unmodified, so what the web page shows is checked against what
 * the CLI configured, through the same bytes the switch would hold.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_port.h"
#include "machine.h"
#include "page_impl.h"
#include "support.h"
#include "hw_mock.h"

extern uint8_t  outbuf[TCP_OUTBUF_SIZE];
extern uint16_t slen;
extern uint8_t  vlan_names[VLAN_NAMES_SIZE];
extern uint16_t vlan_ptr;
extern uint16_t management_vlan;

/* The response body: what follows the blank line after the header. */
static const char *body(void)
{
	outbuf[slen < TCP_OUTBUF_SIZE ? slen : TCP_OUTBUF_SIZE - 1] = 0;
	const char *p = strstr((const char *)outbuf, "\r\n\r\n");
	return p ? p + 4 : (const char *)outbuf;
}

static int has(const char *needle) { return strstr(body(), needle) != 0; }

static int count(const char *needle)
{
	int n = 0;
	for (const char *p = body(); (p = strstr(p, needle)); p += strlen(needle))
		n++;
	return n;
}

/* Brackets and braces balance outside strings, quotes pair up. */
static int balanced(void)
{
	int depth = 0, instr = 0;
	for (const char *p = body(); *p; p++) {
		if (*p == '"') { instr = !instr; continue; }
		if (instr) continue;
		if (*p == '{' || *p == '[') depth++;
		if (*p == '}' || *p == ']') depth--;
		if (depth < 0) return 0;
	}
	return depth == 0 && !instr;
}

static void name_vlan(uint16_t vid, const char *name)
{
	static const char h[] = "0123456789abcdef";
	vlan_names[vlan_ptr++] = h[(vid >> 8) & 0xf];
	vlan_names[vlan_ptr++] = h[(vid >> 4) & 0xf];
	vlan_names[vlan_ptr++] = h[vid & 0xf];
	while (*name) vlan_names[vlan_ptr++] = *name++;
	vlan_names[vlan_ptr++] = ' ';
	vlan_names[vlan_ptr] = 0;
}

static void make_vlan(uint16_t vid, uint16_t members, uint16_t tagged)
{
	vlan_settings.vlan = vid;
	vlan_settings.members = members;
	vlan_settings.tagged = tagged;
	vlan_create();
}

static void t_vlan(void)
{
	printf("[test] /vlan.json shows what vlan_create and pvid wrote\n");
	hw_reset(); out_reset(); vlan_ptr = 0; vlan_names[0] = 0;
	make_vlan(10, 0x0015, 0x0004);
	name_vlan(10, "lab");
	port_pvid_set(0, 10);
	port_pvid_set(2, 10);
	port_pvid_set(4, 20);
	send_vlan(10);
	char want[32];
	snprintf(want, sizeof want, "\"members\":\"0x%x\"", hw_vlan_word(10));
	CHECK(has(want), "members is the entry word");
	CHECK(has("\"name\":\"lab\""), "name comes from the name table");
	CHECK(has("\"pvid\":\"0x0005\""), "pvid mask has ports 1 and 3, not 5");
	CHECK(balanced(), "valid JSON");
}

static void t_vlanlist(void)
{
	printf("[test] /vlanlist lists the VLANs that exist\n");
	hw_reset(); out_reset(); vlan_ptr = 0; vlan_names[0] = 0;
	make_vlan(10, 0x0015, 0x0004);
	make_vlan(20, 0x00f0, 0x0000);
	make_vlan(4094, 0x0001, 0x0000);
	name_vlan(10, "lab");
	name_vlan(4094, "top");
	management_vlan = 10;
	send_vlanlist();
	CHECK(has("\"mgmt\":10"), "management VLAN");
	CHECK(has("{\"id\":10,\"name\":\"lab\"}"), "VLAN 10 with its name");
	CHECK(has("{\"id\":20,\"name\":\"\"}"), "VLAN 20 without a name");
	CHECK(has("{\"id\":4094,\"name\":\"top\"}"), "VLAN 4094 is the last one listed");
	CHECK(count("\"id\":") == 3, "and nothing else");
	CHECK(balanced(), "valid JSON");
	vlan_delete(20);
	send_vlanlist();
	CHECK(count("\"id\":") == 2 && !has("\"id\":20"), "a deleted VLAN drops out");
	management_vlan = 1;
}

static const uint8_t base_mac[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x00 };

static void put(uint16_t idx, uint8_t last, uint16_t vid, uint8_t port, bool st)
{
	uint8_t m[6];
	memcpy(m, base_mac, 6);
	m[5] = last;
	hw_l2_put(idx, m, vid, port, st);
}

static void t_l2(void)
{
	printf("[test] /l2.json walks the table\n");
	hw_reset(); out_reset();
	put(0x010, 0x01, 1, 4, false);
	put(0x020, 0x02, 2, CPU_PORT, true);
	put(0x8f0, 0x03, 1, 7, false);
	send_l2(0);
	CHECK(count("\"mac\":") == 4, "three entries, and the first one again: that repeat is how the page sees the wrap");
	CHECK(count("\"idx\":\"0010\"") == 2 && strstr(body(), "\"idx\":\"0010\"}]"), "the repeat closes the list");
	CHECK(has("{\"vlan\":\"001\",\"mac\":\"00:11:22:33:44:01\",\"type\":\"l\",\"port\":4,\"lag\":0,\"idx\":\"0010\"}"), "learned entry, VLAN, port and index");
	CHECK(has("\"mac\":\"00:11:22:33:44:02\",\"type\":\"s\",\"port\":9"), "static entry on the CPU port");
	CHECK(has("\"idx\":\"08f0\""), "an index above 0x800 keeps its high bit");
	CHECK(balanced(), "valid JSON");

	out_reset();
	send_l2(0x100);
	const char *b = body();
	CHECK(strstr(b, "44:03") && strstr(b, "44:03") < strstr(b, "44:01"), "starting at 0x100 lists 0x8f0 first and wraps");
	CHECK(count("\"mac\":") == 4 && count("44:03") == 2, "and still finds all three before repeating 0x8f0");
}

/* Walk the table the way the page does: ask for the index after the last
 * entry of each page until an index comes back that was seen before. */
static void t_l2_page(void)
{
	printf("[test] /l2.json pages the way the web page walks it\n");
	hw_reset(); out_reset();
	for (int i = 0; i < 40; i++)
		put(0x100 + i * 3, 0x10 + i, 1, i % 8, false);
	uint8_t seen[4096 / 8] = { 0 };
	int pages = 0, unique = 0, wrapped = 0, idx = 0, biggest = 0;
	while (!wrapped && pages < 10) {
		out_reset();
		send_l2(idx);
		pages++;
		if (slen > TCP_OUTBUF_SIZE || !balanced())
			break;
		if ((int)slen > biggest)
			biggest = slen;
		int n = 0, last = -1;
		for (const char *p = body(); (p = strstr(p, "\"idx\":\"")); p += 7) {
			int v = (int)strtol(p + 7, 0, 16);
			n++;
			if (seen[v >> 3] & (1 << (v & 7))) { wrapped = 1; break; }
			seen[v >> 3] |= 1 << (v & 7);
			unique++;
			last = v;
		}
		if (!n)
			break;
		idx = last + 1;
	}
	CHECK(wrapped, "the walk ends on a repeated index");
	CHECK(unique == 40, "and has visited every entry once");
	CHECK(pages == 2, "in two pages");
	CHECK(biggest <= TCP_OUTBUF_SIZE, "no page outgrows outbuf");
	CHECK(balanced(), "every page is valid JSON");
}

static void t_status_counters(void)
{
	printf("[test] /status.json prints 64-bit packet counters in full\n");
	hw_reset(); out_reset();
	hw_counter_set(0, STAT_COUNTER_TX_PKTS, 0x100000005ULL);
	hw_counter_set(0, STAT_COUNTER_RX_PKTS, 0x00000000000000ffULL);
	hw_counter_set(1, STAT_COUNTER_TX_PKTS, 0x00000001ffffffffULL);
	send_status();
	CHECK(has("\"txG\":\"0x100000005\""), "a counter past 2^32 keeps the zeros of its low word");
	CHECK(has("\"rxG\":\"0xff\""), "a small counter stays short");
	CHECK(has("\"txG\":\"0x1ffffffff\""), "a low word of all ones");
	out_reset();
	send_counters(1);
	CHECK(has("\"0x100000005\"") && has("\"0x0\""), "/counters.json prints them the same way");
	CHECK(slen <= TCP_OUTBUF_SIZE, "fits the buffer");
	CHECK(balanced(), "valid JSON");
}

int main(void)
{
	printf("== httpd/page_impl.c JSON tests ==\n");
	t_vlan();
	t_vlanlist();
	t_l2();
	t_l2_page();
	t_status_counters();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

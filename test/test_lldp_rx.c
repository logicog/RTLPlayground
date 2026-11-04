/*
 * test_lldp_rx.c - the LLDP receiver on crafted frames.
 *
 * rtl837x_lldp.c is compiled unmodified; this file carries the frame buffer,
 * the tick counter and the transmit stub it needs, builds LLDPDUs byte by
 * byte and checks what lands in the neighbour table and in /lldp.json.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "machine.h"
#include "page_impl.h"
#include "rtl837x_lldp.h"
#include "support.h"
#include "hw_mock.h"

uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
uint16_t uip_len;
volatile uint32_t ticks;
extern uint8_t outbuf[];
extern uint16_t slen;
extern bool lldp_enabled;
extern const struct machine machine;
static int tx_calls;

void tcpip_output(void) { tx_calls++; }
void memcpyc(uint8_t *dst, const uint8_t *src, uint16_t len) { memcpy(dst, src, len); }
uint16_t strlen_x(const char *s) { return strlen(s); }

#define RX_TAG_LEN 8
#define LLDP_DST "\x01\x80\xc2\x00\x00\x0e"

static uint8_t *frame_start(uint8_t port, int vlan_tag)
{
	uint8_t *p = uip_buf;

	memset(uip_buf, 0, sizeof(uip_buf));
	memcpy(p, LLDP_DST, 6); p += 6;
	memcpy(p, "\xaa\xbb\xcc\xdd\xee\xff", 6); p += 6;
	*p++ = 0x88; *p++ = 0x99; *p++ = 4; *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0; *p++ = port;
	if (vlan_tag) { *p++ = 0x81; *p++ = 0x00; *p++ = 0x00; *p++ = 0x0a; }
	*p++ = 0x88; *p++ = 0xcc;
	return p;
}

static uint8_t *tlv(uint8_t *p, uint8_t type, uint16_t len, const void *v)
{
	*p++ = (type << 1) | (len >> 8);
	*p++ = len & 0xff;
	memcpy(p, v, len);
	return p + len;
}

static uint8_t *chassis_mac(uint8_t *p, const char *mac)
{
	uint8_t v[7] = { 4 };
	memcpy(v + 1, mac, 6);
	return tlv(p, LLDP_CHASSIS_ID_TLV_TYPE, 7, v);
}

static uint8_t *port_name(uint8_t *p, const char *name)
{
	uint8_t v[64] = { 5 };
	memcpy(v + 1, name, strlen(name));
	return tlv(p, LLDP_PORT_ID_TLV_TYPE, 1 + strlen(name), v);
}

static uint8_t *ttl(uint8_t *p, uint16_t secs)
{
	uint8_t v[2] = { secs >> 8, secs & 0xff };
	return tlv(p, LLDP_TTL_TLV_TYPE, 2, v);
}

static uint8_t *end(uint8_t *p) { return tlv(p, LLDP_END_TLV_TYPE, 0, ""); }

static void finish(uint8_t *p) { uip_len = p - uip_buf; }

static int live(void)
{
	int n = 0;
	for (int i = 0; i < LLDP_NB_MAX; i++) if (lldp_nb[i].ttl) n++;
	return n;
}

static struct lldp_neighbor *first(void)
{
	for (int i = 0; i < LLDP_NB_MAX; i++) if (lldp_nb[i].ttl) return &lldp_nb[i];
	return 0;
}

static void simple(uint8_t port, const char *mac, const char *name, const char *sys, uint16_t t)
{
	uint8_t *p = frame_start(port, 0);
	p = chassis_mac(p, mac);
	p = port_name(p, name);
	p = ttl(p, t);
	p = tlv(p, LLDP_SYSNAME_TLV_TYPE, strlen(sys), sys);
	finish(end(p));
}

static void t_basic(void)
{
	lldp_init();
	simple(4, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a", 120);
	CHECK(lldp_in(), "an LLDPDU to the LLDP address is taken");
	CHECK(live() == 1, "one neighbour");
	CHECK(first()->port == 4, "ingress port from the CPU tag");
	CHECK(strcmp(first()->port_id, "eth0") == 0, "port id without the subtype byte");
	CHECK(strcmp(first()->sys_name, "host-a") == 0, "system name");
	CHECK(first()->ttl == 120, "ttl from the frame");
}

static void t_not_ours(void)
{
	lldp_init();
	simple(4, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a", 120);
	uip_buf[5] = 0x00;
	CHECK(!lldp_in(), "another multicast address is not ours");
	simple(4, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a", 120);
	uip_buf[RX_TAG_LEN + 12 + 1] = 0xcd;
	CHECK(!lldp_in(), "another ethertype on the LLDP address is not ours");
	CHECK(live() == 0, "nothing was stored");
}

static void t_vlan_tagged(void)
{
	uint8_t *p = frame_start(5, 1);
	lldp_init();
	p = chassis_mac(p, "\x02\x00\x00\x00\x00\x02");
	p = port_name(p, "ge1");
	p = ttl(p, 60);
	finish(end(p));
	CHECK(lldp_in() && live() == 1, "a frame with a VLAN tag after the CPU tag parses");
	CHECK(strcmp(first()->port_id, "ge1") == 0, "fields found behind the tag");
}

static void t_refresh_and_key(void)
{
	lldp_init();
	simple(4, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a", 120);
	lldp_in();
	simple(4, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a-renamed", 90);
	lldp_in();
	CHECK(live() == 1, "same chassis on the same port updates in place");
	CHECK(strcmp(first()->sys_name, "host-a-renamed") == 0 && first()->ttl == 90, "the update carries the new fields");
	simple(6, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a", 120);
	lldp_in();
	CHECK(live() == 2, "the same chassis on another port is a second entry");
}

static void t_shutdown(void)
{
	lldp_init();
	simple(4, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a", 120);
	lldp_in();
	simple(4, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a", 0);
	lldp_in();
	CHECK(live() == 0, "a TTL of 0 removes the entry");
	simple(4, "\x02\x00\x00\x00\x00\x09", "eth0", "host-x", 0);
	lldp_in();
	CHECK(live() == 0, "a TTL of 0 for an unknown neighbour adds nothing");
}

static void t_malformed(void)
{
	uint8_t *p;
	lldp_init();
	p = frame_start(4, 0);
	p = port_name(p, "eth0");
	p = ttl(p, 120);
	finish(end(p));
	lldp_in();
	CHECK(live() == 0, "no chassis id, no entry");

	p = frame_start(4, 0);
	p = chassis_mac(p, "\x02\x00\x00\x00\x00\x01");
	*p++ = (LLDP_SYSNAME_TLV_TYPE << 1) | 1;
	*p++ = 0xf0;
	finish(p);
	lldp_in();
	CHECK(live() == 1 && first()->sys_name[0] == 0, "a length past the end of the frame stops the walk, earlier TLVs stay");

	lldp_init();
	p = frame_start(4, 0);
	p = chassis_mac(p, "\x02\x00\x00\x00\x00\x01");
	finish(p + 1);
	lldp_in();
	CHECK(live() == 1, "a dangling single byte at the end is ignored");

	lldp_init();
	p = frame_start(4, 0);
	p = chassis_mac(p, "\x02\x00\x00\x00\x00\x01");
	finish(p);
	lldp_in();
	CHECK(live() == 1 && first()->ttl == LLDP_TTL_TTL_SECONDS, "no TTL TLV falls back to the default");
}

static void t_sanitize(void)
{
	char raw[40];
	lldp_init();
	memset(raw, 0, sizeof(raw));
	memcpy(raw, "a\"b\\c\x01" "d\xc3\xa9" "e", 10);
	simple(4, "\x02\x00\x00\x00\x00\x01", raw, "x", 120);
	lldp_in();
	CHECK(strcmp(first()->port_id, "a.b.c.d..e") == 0, "quotes, backslashes, controls and non-ASCII become dots");
	simple(4, "\x02\x00\x00\x00\x00\x01", "0123456789abcdef", "0123456789abcdefghij", 120);
	lldp_in();
	CHECK(strlen(first()->port_id) == LLDP_NB_PORT_ID_LENGTH, "port id is cut to its field");
	CHECK(strlen(first()->sys_name) == LLDP_NB_SYS_NAME_LENGTH, "system name is cut to its field");
}

static void t_nine_bit_length(void)
{
	uint8_t big[300];
	uint8_t *p;
	lldp_init();
	memset(big, 'z', sizeof(big));
	p = frame_start(4, 0);
	p = chassis_mac(p, "\x02\x00\x00\x00\x00\x01");
	p = tlv(p, LLDP_SYSDESC_TLV_TYPE, 300, big);
	p = tlv(p, LLDP_SYSNAME_TLV_TYPE, 5, "after");
	p = ttl(p, 77);
	finish(end(p));
	lldp_in();
	CHECK(live() == 1 && strcmp(first()->sys_name, "after") == 0 && first()->ttl == 77,
	      "a 9-bit TLV length is skipped correctly");
}

static void t_port_range(void)
{
	lldp_init();
	simple(MACHINE_MAX_PORT + 1, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a", 120);
	lldp_in();
	simple(15, "\x02\x00\x00\x00\x00\x02", "eth0", "host-b", 120);
	lldp_in();
	CHECK(live() == 0, "frames from outside the port range are dropped");
}

static void t_pool_and_eviction(void)
{
	char mac[6] = { 2, 0, 0, 0, 0, 0 };
	int i;
	lldp_init();
	for (i = 0; i < LLDP_NB_MAX + 2; i++) {
		mac[5] = i + 1;
		simple(4, mac, "p", "n", 100 + i);
		lldp_in();
	}
	CHECK(live() == LLDP_NB_MAX, "the pool holds LLDP_NB_MAX entries");
	for (i = 0; i < LLDP_NB_MAX; i++)
		if (lldp_nb[i].chassis[5] == 1 || lldp_nb[i].chassis[5] == 2) break;
	CHECK(i == LLDP_NB_MAX, "the two entries closest to expiry were evicted for the newcomers");
}

static void t_ageing(void)
{
	lldp_init();
	lldp_enabled = 1;
	simple(4, "\x02\x00\x00\x00\x00\x01", "eth0", "host-a", 2);
	lldp_in();
	ticks += SYS_TICK_HZ; lldp_tick();
	CHECK(live() == 1 && first()->ttl == 1, "one second step ages the entry");
	ticks += SYS_TICK_HZ; lldp_tick();
	CHECK(live() == 0, "the entry expires when its ttl reaches 0");
}

static void t_json(void)
{
	int i;
	lldp_init();
	simple(4, "\x02\x00\x00\x00\x00\x01", "e\"th", "ho\\st", 120);
	lldp_in();
	slen = 0;
	lldp_neighbors_to_html();
	outbuf[slen] = 0;
	CHECK(strcmp((char *)outbuf, ",\"neighbors\":[{\"port\":5,\"chassis\":\"02:00:00:00:00:01\",\"portId\":\"e.th\",\"sysName\":\"ho.st\",\"ttl\":120}]") == 0,
	      "neighbour JSON shape, port in the physical numbering");
	lldp_init();
	for (i = 0; i < LLDP_NB_MAX; i++) {
		char mac[6] = { 2, 0, 0, 0, 0, i + 1 };
		simple(4, mac, "0123456789ab", "0123456789abcdef", 65535);
		lldp_in();
	}
	slen = TCP_OUTBUF_SIZE - 3 * LLDP_NB_JSON_LENGTH;
	lldp_neighbors_to_html();
	CHECK(slen <= TCP_OUTBUF_SIZE, "the list stops inside the reply buffer");
	CHECK(outbuf[slen - 1] == ']', "and is still closed");
}

static void t_tx_rounds(void)
{
	lldp_init();
	lldp_enabled = 1;
	lldp_logical_port_status = 0xffff;
	tx_calls = 0;
	for (int s = 0; s < LLDP_TX_INTERVAL_S; s++) { ticks += SYS_TICK_HZ; lldp_tick(); }
	CHECK(tx_calls == MACHINE_MAX_PORT - MACHINE_MIN_PORT + 1, "one frame per port after the interval");
	lldp_enabled = 0;
}

int main(void)
{
	printf("== rtl837x_lldp.c receiver tests ==\n");
	hw_reset();
	hw_reg_set(RTL837X_REG_LINKS_STS, 0xffffffff);
	t_basic();
	t_not_ours();
	t_vlan_tagged();
	t_refresh_and_key();
	t_shutdown();
	t_malformed();
	t_sanitize();
	t_nine_bit_length();
	t_port_range();
	t_pool_and_eviction();
	t_ageing();
	t_json();
	t_tx_rounds();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

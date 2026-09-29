/*
 * test_dhcp_snoop.c - rtl837x_dhcp_snoop.c against the simulated table engine.
 *
 * The three ACL rules are compared with words built from the measured encoding,
 * and DHCP frames laid out as the NIC delivers them (MACs, RTL tag, inserted
 * 802.1Q tag, IPv4, UDP, BOOTP) are fed to dhcp_snoop_in() to follow a binding
 * from request to ACK to release.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_acl.h"
#include "rtl837x_dhcp_snoop.h"
#include "support.h"
#include "hw_mock.h"

uint8_t uip_buf[UIP_CONF_BUFFER_SIZE + 2];
uint16_t uip_len;

void print_mac(uint8_t *m)
{
	char b[20];
	snprintf(b, sizeof(b), "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
	print_string(b);
}

void print_ip(uint8_t *ip)
{
	char b[16];
	snprintf(b, sizeof(b), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
	print_string(b);
}

#define PORT_CLIENT	3
#define PORT_SERVER	8
#define VID		2

static const uint8_t client_mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0xc1};

static uint8_t *opt;

static void frame(uint8_t port, uint8_t op, uint16_t sport, uint16_t dport, uint8_t msg)
{
	uint8_t *p = uip_buf;

	memset(uip_buf, 0, sizeof(uip_buf));
	memset(p, 0xff, 6);
	memcpy(p + 6, client_mac, 6);
	p += 12;
	p[0] = 0x88; p[1] = 0x99; p[2] = 0x04; p[7] = port;
	p += 8;
	p[0] = 0x81; p[1] = 0x00; p[2] = VID >> 8; p[3] = VID & 0xff;
	p += 4;
	p[0] = 0x08; p[1] = 0x00;
	p += 2;
	p[0] = 0x45; p[9] = 17;
	p += 20;
	p[0] = sport >> 8; p[1] = sport & 0xff; p[2] = dport >> 8; p[3] = dport & 0xff;
	p += 8;
	p[0] = op; p[1] = 1; p[2] = 6;
	if (op == 2) {
		p[16] = 10; p[17] = 99; p[18] = 0; p[19] = 50;
	}
	memcpy(p + 28, client_mac, 6);
	p[236] = 0x63; p[237] = 0x82; p[238] = 0x53; p[239] = 0x63;
	opt = p + 240;
	*opt++ = 53; *opt++ = 1; *opt++ = msg;
	uip_len = opt + 1 - uip_buf;
	*opt = 255;
}

static void lease(uint32_t secs)
{
	*opt++ = 51; *opt++ = 4;
	*opt++ = secs >> 24; *opt++ = secs >> 16; *opt++ = secs >> 8; *opt++ = secs;
	*opt = 255;
	uip_len = opt + 1 - uip_buf;
}

static struct dhcp_binding *entry(void)
{
	for (int i = 0; i < DHCP_BIND_MAX; i++)
		if (dhcp_bind[i].state != DHCP_BIND_FREE && !memcmp(dhcp_bind[i].mac, client_mac, 6))
			return &dhcp_bind[i];
	return NULL;
}

static int in_ports(uint8_t idx)
{
	return (hw_acl_rule(0x80 | idx, 4) >> 11) & 0x3ff;
}

static void t_rules(void)
{
	printf("[test] snooping rules\n");
	hw_reset();
	dhcp_snoop_trust = 1 << PORT_SERVER;
	dhcp_snoop_on = 1;
	dhcp_snoop_apply();

	CHECK(hw_acl_rule(0x80 | DHCP_SNOOP_RULE_DROP, 2) == 0x0043ffff && hw_acl_rule(DHCP_SNOOP_RULE_DROP, 2) == 0xffbcffff,
	      "rule 65 matches source port 67 in template 1");
	CHECK((hw_acl_rule(0x80 | DHCP_SNOOP_RULE_DROP, 4) & 0x7ff) == (0x400 | 0x080 | 0x038 | 1)
	      && (hw_acl_rule(DHCP_SNOOP_RULE_DROP, 4) & 0x7ff) == (0x300 | 0x040 | 0x038 | 6),
	      "rule 65 is IPv4 UDP, template 1");
	CHECK(in_ports(DHCP_SNOOP_RULE_DROP) == 0x0ff, "rule 65 applies to the untrusted ports");
	CHECK(hw_acl_act(DHCP_SNOOP_RULE_DROP, 1) == 0x00020000
	      && hw_reg_get(RTL837X_ACL_ACT_CTRL + 4 * DHCP_SNOOP_RULE_DROP) == 0x20, "rule 65 drops");

	CHECK(hw_acl_rule(0x80 | DHCP_SNOOP_RULE_SERVER, 2) == 0x0043ffff, "rule 66 matches source port 67");
	CHECK(in_ports(DHCP_SNOOP_RULE_SERVER) == 0x100, "rule 66 applies to the trusted port");
	CHECK(hw_acl_act(DHCP_SNOOP_RULE_SERVER, 1) == 0x40000000
	      && hw_reg_get(RTL837X_ACL_ACT_CTRL + 4 * DHCP_SNOOP_RULE_SERVER) == 0x20, "rule 66 copies to the CPU port");

	CHECK(hw_acl_rule(0x80 | DHCP_SNOOP_RULE_CLIENT, 2) == 0x0044ffff, "rule 67 matches source port 68");
	CHECK(in_ports(DHCP_SNOOP_RULE_CLIENT) == 0x0ff, "rule 67 applies to the untrusted ports");
	CHECK(hw_acl_act(DHCP_SNOOP_RULE_CLIENT, 1) == 0x40000000, "rule 67 copies to the CPU port");
	CHECK(hw_reg_get(RTL837X_ACL_PORT_EN) == 0x1ff, "ACL on every front port");

	dhcp_snoop_on = 0;
	dhcp_snoop_apply();
	CHECK(hw_acl_rule(0x80 | DHCP_SNOOP_RULE_DROP, 4) == 0 && hw_acl_act(DHCP_SNOOP_RULE_SERVER, 1) == 0
	      && hw_reg_get(RTL837X_ACL_ACT_CTRL + 4 * DHCP_SNOOP_RULE_CLIENT) == 0xff,
	      "off clears the three rules");

	dhcp_snoop_on = 1;
	dhcp_snoop_trust = 0;
	dhcp_snoop_apply();
	CHECK(hw_acl_rule(0x80 | DHCP_SNOOP_RULE_DROP, 4) == 0, "on without a trusted port filters nothing");
}

static void t_bindings(void)
{
	struct dhcp_binding *b;

	printf("[test] snooping bindings\n");
	hw_reset();
	dhcp_snoop_trust = 1 << PORT_SERVER;
	dhcp_snoop_on = 1;
	dhcp_snoop_apply();

	frame(PORT_CLIENT, 1, 68, 67, 1);
	dhcp_snoop_in();
	b = entry();
	CHECK(b && b->state == DHCP_BIND_PENDING && b->port == PORT_CLIENT && b->vid == VID,
	      "a DISCOVER from an untrusted port creates a pending entry with port and VLAN");

	frame(PORT_CLIENT, 2, 67, 68, 5);
	lease(120);
	dhcp_snoop_in();
	b = entry();
	CHECK(b && b->state == DHCP_BIND_PENDING, "an ACK from an untrusted port is ignored");

	frame(PORT_SERVER, 2, 67, 68, 5);
	lease(120);
	dhcp_snoop_in();
	b = entry();
	CHECK(b && b->state == DHCP_BIND_BOUND && b->secs == 120 && b->ip[0] == 10 && b->ip[3] == 50,
	      "an ACK from the trusted port binds the address and lease");

	dhcp_snoop_tick();
	CHECK(b && b->secs == 119, "the lease ages once per tick");

	frame(PORT_SERVER, 1, 68, 67, 3);
	dhcp_snoop_in();
	CHECK(b && entry() == b && b->state == DHCP_BIND_BOUND, "a request from the trusted port is not recorded");

	frame(PORT_CLIENT, 1, 68, 67, 7);
	dhcp_snoop_in();
	CHECK(entry() == NULL, "a RELEASE removes the entry");

	frame(PORT_CLIENT, 1, 68, 67, 1);
	dhcp_snoop_in();
	frame(PORT_SERVER, 2, 67, 68, 5);
	lease(120);
	dhcp_snoop_in();
	frame(PORT_SERVER, 2, 67, 68, 6);
	dhcp_snoop_in();
	CHECK(entry() == NULL, "a NAK from the trusted port removes the entry");

	frame(PORT_CLIENT, 1, 68, 67, 1);
	dhcp_snoop_in();
	frame(PORT_SERVER, 2, 67, 68, 5);
	lease(120);
	dhcp_snoop_in();
	frame(PORT_CLIENT, 1, 68, 67, 4);
	dhcp_snoop_in();
	CHECK(entry() == NULL, "a DECLINE removes the entry");

	frame(PORT_CLIENT, 1, 68, 67, 1);
	dhcp_snoop_in();
	frame(PORT_SERVER, 2, 67, 68, 5);
	dhcp_snoop_in();
	b = entry();
	CHECK(b && b->state == DHCP_BIND_BOUND && b->secs == DHCP_LEASE_INFINITE,
	      "an ACK without a lease time binds for good");
	dhcp_snoop_tick();
	CHECK(b && b->secs == DHCP_LEASE_INFINITE, "an infinite lease does not age");
	frame(PORT_CLIENT, 1, 68, 67, 7);
	dhcp_snoop_in();

	frame(PORT_CLIENT, 1, 68, 67, 1);
	uip_buf[12 + 8 + 4 + 2 + 9] = 6;
	dhcp_snoop_in();
	CHECK(entry() == NULL, "a frame that is not UDP is ignored");

	frame(PORT_CLIENT, 1, 68, 67, 1);
	dhcp_snoop_in();
	for (int i = 0; i < DHCP_PENDING_SECS; i++)
		dhcp_snoop_tick();
	CHECK(entry() == NULL, "a pending entry without an ACK expires");
}

static void t_show(void)
{
	static const struct { uint32_t secs; const char *text; } leases[] = {
		{118, "lease 118 s\n"}, {65535, "lease 65535 s\n"}, {65536, "lease 1092 min\n"},
		{262143, "lease 4369 min\n"}, {262144, "lease 72 h\n"}, {1048575, "lease 291 h\n"},
		{1048576, "lease 12 d\n"}, {8388607, "lease 97 d\n"}, {8388608, "lease > 97 d\n"},
		{DHCP_LEASE_INFINITE, "lease infinite\n"},
	};

	printf("[test] snooping binding display\n");
	for (unsigned n = 0; n < sizeof(leases) / sizeof(leases[0]); n++) {
		char msg[80];
		memset(dhcp_bind, 0, sizeof(dhcp_bind));
		memcpy(dhcp_bind[0].mac, client_mac, 6);
		dhcp_bind[0].ip[0] = 10; dhcp_bind[0].ip[1] = 99; dhcp_bind[0].ip[3] = 50;
		dhcp_bind[0].vid = VID;
		dhcp_bind[0].port = PORT_CLIENT;
		dhcp_bind[0].state = DHCP_BIND_BOUND;
		dhcp_bind[0].secs = leases[n].secs;
		out_reset();
		dhcp_snoop_show();
		snprintf(msg, sizeof(msg), "a lease of %u s shows as %.*s", (unsigned)leases[n].secs,
			 (int)strlen(leases[n].text) - 1, leases[n].text);
		CHECK(strstr(out_buf, "02:00:00:00:00:c1 10.99.0.50 vlan 2 port 4 ") && strstr(out_buf, leases[n].text), msg);
	}
}

int main(void)
{
	printf("== rtl837x_dhcp_snoop.c tests ==\n");
	t_rules();
	t_bindings();
	t_show();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

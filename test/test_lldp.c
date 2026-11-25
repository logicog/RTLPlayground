/*
 * test_lldp.c - host unit tests for rtl837x_lldp.c (RX parser, neighbor table,
 * aging, link-down expiry, shutdown LLDPDU, and TX frame generation).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_lldp.h"
#include "machine.h"
#include "timer.h"
#include "uip.h"

/* ---- minimal firmware environment ---- */
uint8_t  uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
uint16_t uip_len;
uip_ipaddr_t uip_hostaddr = { HTONS(0xc000), HTONS(0x0201) };
struct uip_eth_addr uip_ethaddr = { .addr = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 } };
uint8_t  sfr_data[4];
uint16_t SFR_DATA_U16;
bool     lldp_enabled = true;
char     hostname[24] = "switch_test";
char     port_names[9][PORT_NAME_SIZE];
uint32_t ticks;

const struct machine machine = {
	.machine_name = "HOSTTEST",
	.n_sfp = 0,
	.log_to_phys_port = { 1, 2, 3, 4, 5, 6, 7, 8, 9 },
	.phys_to_log_port = { 0, 1, 2, 3, 4, 5, 6, 7, 8 },
};

static uint16_t mock_links_mask = 0x01ff;
static int tx_count = 0;

void reg_read_m(uint16_t reg)
{
	memset(sfr_data, 0, sizeof(sfr_data));
	if (reg == RTL837X_REG_LINKS_STS) {
		sfr_data[1] = (uint8_t)(mock_links_mask & 0xff);
		sfr_data[2] = (uint8_t)(mock_links_mask >> 8);
	}
}
void phy_read(uint8_t phy_id, uint8_t dev_id, uint16_t reg) { (void)phy_id; (void)dev_id; (void)reg; }
uint16_t port_pvid_get(uint8_t port) { (void)port; return 1; }
uint8_t port_lag_of(uint8_t port) { (void)port; return 0xff; }
void tcpip_output(void) { tx_count++; }
void print_string(const char *s) { (void)s; }
void print_string_x(char *s) { (void)s; }
void write_char(char c) { (void)c; }
void itoa(uint8_t v) { (void)v; }
void itoa_short(uint16_t v) { (void)v; }
void print_phys_port(uint8_t p) { (void)p; }
void memcpyc(uint8_t *dst, const uint8_t *src, uint16_t len) { memcpy(dst, src, len); }
uint16_t strlen_x(const char *s) { return (uint16_t)strlen(s); }

static int tests_run, tests_failed;
#define CHECK(cond, msg) do { \
	tests_run++; \
	if (!(cond)) { printf("  FAIL: %s (line %d)\n", msg, __LINE__); tests_failed++; } \
} while (0)

static uint8_t *put_tlv(uint8_t *p, uint8_t type, const void *val, uint16_t len)
{
	uint16_t hdr = ((uint16_t)type << 9) | (len & 0x01ff);
	*p++ = (uint8_t)(hdr >> 8);
	*p++ = (uint8_t)(hdr & 0xff);
	if (len)
		memcpy(p, val, len);
	return p + len;
}

static uint8_t *start_rx_frame(uint8_t log_port, int with_vlan)
{
	uint8_t *p = uip_buf;
	memset(uip_buf, 0, sizeof(uip_buf));
	/* dst 01:80:c2:00:00:0e */
	p[0] = 0x01; p[1] = 0x80; p[2] = 0xc2; p[3] = 0x00; p[4] = 0x00; p[5] = 0x0e;
	/* src */
	p[6] = 0xaa; p[7] = 0xbb; p[8] = 0xcc; p[9] = 0xdd; p[10] = 0xee; p[11] = 0xff;
	/* RTL CPU tag: 88 99 ver reason flags(2) pmask(2) */
	p[12] = 0x88; p[13] = 0x99; p[14] = 0x04; p[15] = 0x00;
	p[16] = 0x00; p[17] = 0x00;
	p[18] = 0x00; p[19] = log_port;
	p += 20;
	if (with_vlan) {
		p[0] = 0x81; p[1] = 0x00; p[2] = 0x00; p[3] = 0x01;
		p += 4;
	}
	p[0] = 0x88; p[1] = 0xcc;
	return p + 2;
}

static void t_rx_basic(void)
{
	printf("[test] lldp_in decodes standard TLVs into neighbor table\n");
	lldp_init();
	uint8_t *p = start_rx_frame(3, 0);
	uint8_t chassis[7] = { 4, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02 };
	uint8_t portid[7]  = { 5, 'P', 'o', 'r', 't', ' ', '1' };
	uint8_t ttl[2]     = { 0x00, 120 };
	uint8_t caps[4]    = { 0x00, 0x14, 0x00, 0x04 };
	uint8_t mgmt[12]   = { 5, 1, 192, 0, 2, 20, 1, 0, 0, 0, 0, 0 };
	p = put_tlv(p, 1, chassis, sizeof(chassis));
	p = put_tlv(p, 2, portid, sizeof(portid));
	p = put_tlv(p, 3, ttl, sizeof(ttl));
	p = put_tlv(p, 4, "uplink", 6);
	p = put_tlv(p, 5, "peer-switch", 11);
	p = put_tlv(p, 7, caps, sizeof(caps));
	p = put_tlv(p, 8, mgmt, sizeof(mgmt));
	p = put_tlv(p, 0, NULL, 0);
	uip_len = (uint16_t)(p - uip_buf);

	lldp_in();
	CHECK(lldp_rx_frames == 1, "rx_frames incremented");
	CHECK(lldp_rx_discarded == 0, "no discards");
	CHECK(lldp_neighbors[3].ttl == 120, "ttl stored");
	CHECK(strcmp(lldp_neighbors[3].chassis, "02:00:00:00:00:02") == 0, "MAC chassis formatted");
	CHECK(strcmp(lldp_neighbors[3].port_id, "Port 1") == 0, "port ID decoded");
	CHECK(strcmp(lldp_neighbors[3].port_desc, "uplink") == 0, "port desc decoded");
	CHECK(strcmp(lldp_neighbors[3].sys_name, "peer-switch") == 0, "sys name decoded");
	CHECK(lldp_neighbors[3].caps_enabled == 0x0004, "enabled caps decoded");
	CHECK(lldp_neighbors[3].mgmt_ip[0] == 192 && lldp_neighbors[3].mgmt_ip[3] == 20, "mgmt IPv4 decoded");
}

static void t_rx_shutdown_and_malformed(void)
{
	printf("[test] malformed frame preserves neighbor; TTL=0 shutdown clears it\n");
	/* Send truncated frame on port 3: must not clobber existing neighbor */
	uint8_t *p = start_rx_frame(3, 1);
	uint8_t chassis[7] = { 4, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
	p = put_tlv(p, 1, chassis, sizeof(chassis));
	/* Missing Port ID and TTL */
	uip_len = (uint16_t)(p - uip_buf);
	lldp_in();
	CHECK(lldp_rx_discarded == 1, "malformed frame discarded");
	CHECK(lldp_neighbors[3].ttl == 120, "existing neighbor preserved");

	/* Send shutdown LLDPDU (TTL=0) on VLAN-tagged frame */
	p = start_rx_frame(3, 1);
	uint8_t portid[5] = { 5, 'e', 't', 'h', '0' };
	uint8_t ttl0[2]   = { 0, 0 };
	p = put_tlv(p, 1, chassis, sizeof(chassis));
	p = put_tlv(p, 2, portid, sizeof(portid));
	p = put_tlv(p, 3, ttl0, sizeof(ttl0));
	p = put_tlv(p, 0, NULL, 0);
	uip_len = (uint16_t)(p - uip_buf);
	lldp_in();
	CHECK(lldp_neighbors[3].ttl == 0, "TTL=0 cleared neighbor immediately");
}

static void t_aging_and_link_down(void)
{
	printf("[test] lldp_tick ages neighbors and expires on link down\n");
	lldp_init();
	mock_links_mask = 0x01ff;
	lldp_neighbors[1].ttl = 2;
	lldp_neighbors[2].ttl = 50;

	/* Advance 1 second: port 1 -> 1, port 2 -> 49 */
	ticks += SYS_TICK_HZ;
	lldp_tick();
	CHECK(lldp_neighbors[1].ttl == 1, "port 1 decremented to 1");
	CHECK(lldp_neighbors[2].ttl == 49, "port 2 decremented to 49");

	/* Drop link on port 2, advance 1 second: both expire */
	mock_links_mask &= ~(1u << 2);
	ticks += SYS_TICK_HZ;
	lldp_tick();
	CHECK(lldp_neighbors[1].ttl == 0, "port 1 aged out");
	CHECK(lldp_neighbors[2].ttl == 0, "port 2 expired on link down");
	CHECK(lldp_rx_ageouts == 2, "two ageouts recorded");
}

int main(void)
{
	printf("== rtl837x_lldp.c tests ==\n");
	t_rx_basic();
	t_rx_shutdown_and_malformed();
	t_aging_and_link_down();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

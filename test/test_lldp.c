#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "machine.h"
#include "uip.h"
#include "rtl837x_lldp.h"

uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
uint16_t uip_len;
struct uip_eth_addr uip_ethaddr = { .addr = {0x02,0x11,0x22,0x33,0x44,0x55} };
const struct machine machine = { .min_port = 0, .max_port = 8,
			   .log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9} };
char hostname[24] = "rtlplayground";
uint8_t outbuf[2500];
uint16_t slen;
volatile uint32_t ticks;
bool lldp_enabled;
uint8_t sfr_data[4];

static uint32_t links_sts;
static int reg_reads, tx_frames;

void print_string(const char *s) { (void)s; }
void write_char(char c) { (void)c; }
void tcpip_output(void) { tx_frames++; }

void reg_read_m(uint16_t reg_addr)
{
	if (reg_addr == RTL837X_REG_LINKS_STS)
		reg_reads++;
	sfr_data[0] = 0;
	sfr_data[1] = links_sts;
	sfr_data[2] = links_sts >> 8;
	sfr_data[3] = 0;
}

void memcpyc(uint8_t *dst, const uint8_t *src, uint16_t len) { memcpy(dst, src, len); }
uint16_t strlen_x(const char *s) { return strlen(s); }
uint16_t strtox(uint8_t *dst, const char *s) { uint16_t n = strlen(s); memcpy(dst, s, n); return n; }

static int failures;
#define CHECK(cond, name) do { \
	if (cond) printf("PASS  %s\n", name); \
	else { printf("FAIL  %s\n", name); failures++; } \
} while (0)

static void step(int seconds)
{
	while (seconds--) {
		ticks += SYS_TICK_HZ;
		lldp_tick();
	}
}

static void put(uint8_t slot, uint8_t port, uint16_t ttl)
{
	lldp_nb[slot].port = port;
	lldp_nb[slot].ttl = ttl;
}

static void reset(bool enabled, uint16_t links)
{
	ticks = 0;
	lldp_init();
	lldp_enabled = enabled;
	links_sts = links;
	reg_reads = tx_frames = 0;
}

int main(void)
{
	uint8_t i;
	int busy = 0;

	reset(true, 0x0108);
	put(0, 3, 120);
	put(1, 8, 120);
	step(5);
	CHECK(lldp_nb[0].ttl == 115 && lldp_nb[1].ttl == 115 && reg_reads == 5,
	      "L1 link up on ports 3 and 8: one second off the TTL per step");

	links_sts = 0x0100;
	step(1);
	CHECK(lldp_nb[0].ttl == 0 && lldp_nb[1].ttl == 114,
	      "L2 port 3 loses its link: its entry is freed, port 8 keeps counting");

	links_sts = 0x0008;
	step(1);
	CHECK(lldp_nb[1].ttl == 0, "L3 port 8 loses its link: the second byte of the mask is read");

	reset(true, 0x0001);
	put(0, 0, 2);
	put(1, 0, 0);
	step(2);
	CHECK(lldp_nb[0].ttl == 0 && lldp_nb[1].ttl == 0, "L4 TTL runs out with the link up, a free slot stays free");

	reset(false, 0x01ff);
	put(0, 3, 120);
	step(LLDP_TX_INTERVAL_S + LLDP_TX_INTERVAL_S / 2);
	CHECK(reg_reads == 0 && tx_frames == 0 && lldp_nb[0].ttl == 120,
	      "L5 LLDP off: no link read, no ageing, nothing sent");

	lldp_enabled = true;
	step(LLDP_TX_INTERVAL_S - 1);
	CHECK(tx_frames == 0, "L6 after lldp on, nothing is sent before the interval");
	step(1);
	CHECK(tx_frames > 0, "L7 the first transmission comes one interval after lldp on");

	for (i = 0; i < LLDP_NB_MAX; i++)
		put(i, 3, 120);
	lldp_init();
	for (i = 0; i < LLDP_NB_MAX; i++)
		busy += lldp_nb[i].ttl != 0;
	CHECK(busy == 0, "L8 lldp off frees every slot of the table");

	printf("\n%s (%d failure%s)\n", failures ? "SANDBOX: FAILURES" : "SANDBOX: ALL PASS",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}

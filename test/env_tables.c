/*
 * env_tables.c - what rtl837x_port.c and httpd/page_impl.c link against
 * besides the register mock: the console, the buffers page_impl writes into,
 * the machine description and leaf calls into subsystems not under test.
 * Buffers keep their firmware sizes so AddressSanitizer sees the same bounds
 * the 8051 has.
 */
#include <stdint.h>
#include <stdio.h>

#include "rtl837x_common.h"
#include "rtl837x_port.h"
#include "machine.h"
#include "uip.h"
#include "rtl837x_flash.h"
#include "syslog.h"
#include "rtl837x_phy.h"
#include "support.h"

/* ---- console: everything lands in out_buf (support.c) ---- */
void print_byte(uint8_t v)
{
	static const char h[] = "0123456789abcdef";
	write_char(h[(v >> 4) & 0xf]);
	write_char(h[v & 0xf]);
}
void print_short(uint16_t v) { print_byte(v >> 8); print_byte(v); }
void print_long(uint32_t v)  { print_short(v >> 16); print_short(v); }
void print_reg(uint16_t v)   { print_short(v); }
void itoa_short(uint16_t v)
{
	char b[6]; int n = 0;
	do { b[n++] = '0' + v % 10; v /= 10; } while (v);
	while (n) write_char(b[--n]);
}
void dbg_string(char *p) { (void)p; }
void dbg_short(uint16_t v) { (void)v; }
void dbg_char(char c) { (void)c; }
void dbg_byte(uint8_t v) { (void)v; }

/* ---- copied verbatim from rtlplayground.c ---- */
uint16_t strtox(uint8_t *dst, const char *s)
{
	uint8_t *b = dst;
	while (*s)
		*dst++ = *s++;
	*dst = 0;
	return dst - b;
}

const uint16_t bit_mask[16] = {
	0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
	0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000
};
const uint8_t hex_chars[] = "0123456789abcdef";
uint8_t *hex = (uint8_t *)hex_chars;

/* ---- a 9-port RTL8373 whose logical ports map to themselves ---- */
const struct machine machine = {
	.machine_name = "HOSTTEST",
	.min_port = 0,
	.max_port = 8,
	.n_sfp = 0,
	.log_to_phys_port = { 1, 2, 3, 4, 5, 6, 7, 8, 9 },
	.phys_to_log_port = { 0, 1, 2, 3, 4, 5, 6, 7, 8 },
};
struct machine_runtime machine_detected = { .isRTL8373 = 1 };

/* ---- firmware state the modules read or write ---- */
uint8_t  outbuf[TCP_OUTBUF_SIZE];
uint16_t slen;
uint16_t management_vlan = 1;
uint16_t cont_len;
uint32_t cont_addr;
uint8_t  vlan_names[VLAN_NAMES_SIZE];
uint16_t vlan_ptr;
uint8_t  sfp_pins_last = 0xff;
uint8_t  sfp_options[2];
char     sfp_module_vendor[2][17];
char     sfp_module_model[2][17];
char     sfp_module_serial[2][17];
char     hostname[24] = "hosttest";
char     port_names[9][PORT_NAME_SIZE];
struct flash_region_t flash_region;
struct syslog_state syslog_state;
bool     stp_enabled;

uip_ipaddr_t uip_hostaddr, uip_draddr, uip_netmask;
struct uip_eth_addr uip_ethaddr = { .addr = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 } };

/* ---- leaf calls into subsystems not under test ---- */
void     flash_read_bulk(uint8_t *dst) { (void)dst; }
char    *get_flash_size_str(void) { return (char *)"2M"; }
uint8_t  sfp_read_reg(uint8_t slot, uint8_t reg) { (void)slot; (void)reg; return 0; }
bool     gpio_pin_test(uint8_t pin) { (void)pin; return false; }
void     phy_read(uint8_t phy_id, uint8_t dev_id, uint16_t reg) { (void)phy_id; (void)dev_id; (void)reg; }
void     phy_write(uint8_t phy_id, uint8_t dev_id, uint16_t reg, uint16_t val) { (void)phy_id; (void)dev_id; (void)reg; (void)val; }
void     phy_reset(uint8_t port) { (void)port; }
uint8_t  stp_port_role(uint8_t port) { (void)port; return 0; }
uint8_t  stp_port_state(uint8_t port) { (void)port; return 3; }

/* ---- STP state send_stp() prints; the tree is not under test here ---- */
#include "rtl837x_stp.h"
struct bridge root_bridge;
struct bridge stp_dbridge[STP_ENTITIES];
uint16_t stp_bpdu_age[STP_ENTITIES];
uint16_t stp_dpid[STP_ENTITIES];
uint16_t stp_lag_mask[STP_LAG_COUNT];
uint16_t stp_tc_count;
uint32_t root_bridge_cost;
uint32_t stp_dcost[STP_ENTITIES];
uint32_t stp_pcost[STP_ENTITIES];
uint8_t  stp_ent_of[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
uint8_t  stp_fwddelay_s = 15, stp_hello_s = 2, stp_maxage_s = 20;
uint8_t  stp_pflags[STP_ENTITIES];
uint8_t  stp_pp2p[STP_ENTITIES];
uint8_t  stp_pprio[STP_ENTITIES];
uint8_t  stp_prio = 0x80, stp_root_port = 0xff, stp_rstp = 1, stp_txhold = 6;

/* ---- SFP: no module present, reads fail ---- */
uint8_t sfp_buf[16];
bool    sfp_read_block(uint8_t slot, uint8_t reg, uint8_t len) { (void)slot; (void)reg; (void)len; return false; }
void    print_phys_port(uint8_t port) { itoa_short(machine.log_to_phys_port[port]); }

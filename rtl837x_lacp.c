/*
 * LACP (IEEE 802.3ad Clause 43) driver for the RTL837x platform.
 * This code is in the Public Domain.
 */

// #define DEBUG

#pragma codeseg BANK3
#pragma constseg BANK3

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_lacp.h"
#include "rtl837x_trap.h"
#include "rtl837x_port.h"
#include "uip.h"
#include "machine.h"

extern __code const struct machine machine;
extern __xdata struct machine_runtime machine_detected;
extern __xdata uint16_t management_vlan;
extern __xdata struct uip_eth_addr uip_ethaddr;
extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];

/* 20-byte Actor/Partner information body (802.3ad 43.4.2.2), network byte order */
struct lacp_info {
	uint16_t sys_prio;
	uint8_t  sys[MAC_ADDR_LEN];
	uint16_t key;
	uint16_t port_prio;
	uint16_t port;
	uint8_t  state;
	uint8_t  reserved[3];
};

/* Outgoing LACPDU (no VLAN tag on TX, as in stp_pkt) */
struct lacpdu {
	uint8_t  dst[MAC_ADDR_LEN];
	uint8_t  src[MAC_ADDR_LEN];
	struct rtl_tag rtl_tag;
	uint16_t ethertype;
	uint8_t  subtype;
	uint8_t  version;
	uint8_t  tlv_actor;
	uint8_t  actor_len;
	struct lacp_info actor;
	uint8_t  tlv_partner;
	uint8_t  partner_len;
	struct lacp_info partner;
	uint8_t  tlv_collector;
	uint8_t  collector_len;
	uint16_t collector_max_delay;
	uint8_t  collector_reserved[12];
	uint8_t  tlv_terminator;
	uint8_t  terminator_len;
	uint8_t  terminator_reserved[50];
};

/* Incoming LACPDU, split because a VLAN tag sits between the two only when the
 * frame was forwarded: a trapped one carries none (cf. stp_pkt_in) */
struct lacpdu_in_hdr {
	uint8_t  dst[MAC_ADDR_LEN];
	uint8_t  src[MAC_ADDR_LEN];
	struct rtl_tag rtl_tag;
};

struct lacpdu_in {
	uint16_t ethertype;
	uint8_t  subtype;
	uint8_t  version;
	uint8_t  tlv_actor;
	uint8_t  actor_len;
	struct lacp_info actor;
	uint8_t  tlv_partner;
	uint8_t  partner_len;
	struct lacp_info partner;
};

__xdata uint8_t lacp_rxbody;

#define LACP_O ((__xdata struct lacpdu        *)&uip_buf[RTL_FRAME_DESC_SIZE])
#define LACP_H ((__xdata struct lacpdu_in_hdr *)&uip_buf[0])
#define LACP_I ((__xdata struct lacpdu_in     *)&uip_buf[lacp_rxbody])

__xdata uint8_t  lacp_actor_state[10];
__xdata uint8_t  lacp_rx_state[10];
__xdata uint8_t  lacp_partner_state[10];
__xdata uint8_t  lacp_partner_sys[10][MAC_ADDR_LEN];
__xdata uint16_t lacp_partner_key[10];
__xdata uint16_t lacp_partner_port[10];
__xdata uint16_t lacp_partner_sys_prio[10];
__xdata uint16_t lacp_partner_port_prio[10];
__xdata uint16_t lacp_periodic[10];
__xdata uint16_t lacp_timeout[10];
__xdata uint8_t  lacp_ntt[10];

#define LACP_TICK_STEP	(SYS_TICK_HZ / 50)
#define LACP_CATCH_UP	8
__xdata uint8_t lacp_tick_last;
extern volatile __xdata uint32_t ticks;

__xdata uint16_t lacp_lag_ports[LACP_NUM_LAGS];
__xdata uint8_t  lacp_port_lag[10];
__xdata uint8_t  lacp_agg_sys[LACP_NUM_LAGS][MAC_ADDR_LEN];
__xdata uint8_t  lacp_agg_valid[LACP_NUM_LAGS];
__xdata uint16_t lacp_members_last[LACP_NUM_LAGS];

__xdata uint16_t lacp_scratch_mask;
__xdata uint8_t  lacp_scratch_flag;
__xdata uint8_t  lacp_scratch_lag;

__xdata uint16_t lacp_rx_count[10];


static uint8_t lacp_sys_eq(__xdata uint8_t *a, __xdata uint8_t *b)
{
	for (uint8_t i = 0; i < MAC_ADDR_LEN; i++) {
		if (a[i] != b[i])
			return 0;
	}
	return 1;
}


static uint8_t lacp_port_selected(uint8_t port)
{
	uint8_t lag = lacp_port_lag[port];
	if (lag == LACP_LAG_NONE)
		return 0;
	if (lacp_rx_state[port] != LACP_RX_CURRENT)
		return 0;
	if (!(lacp_partner_state[port] & LACP_STATE_AGGREGATION))
		return 0;
	if (!lacp_agg_valid[lag])
		return 0;
	return lacp_sys_eq(lacp_partner_sys[port], lacp_agg_sys[lag]);
}


static uint8_t lacp_mux_machine(uint8_t port)
{
	uint8_t old = lacp_actor_state[port];
	uint8_t new = old;

	if (lacp_port_selected(port)) {
		new |= LACP_STATE_SYNC;
		if (lacp_partner_state[port] & LACP_STATE_SYNC)
			new |= LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING;
		else
			new &= ~(LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING);
	} else {
		new &= ~(LACP_STATE_SYNC | LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING);
	}

	if (new == old)
		return 0;
	lacp_actor_state[port] = new;
	return 1;
}

void lacp_mux_update(void) __banked
{
	for (uint8_t lag = 0; lag < LACP_NUM_LAGS; lag++) {
		if (!lacp_lag_ports[lag])
			continue;
		lacp_scratch_mask = 0;
		for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
			if (lacp_port_lag[i] != lag)
				continue;
			if ((lacp_actor_state[i] & LACP_STATE_FULL) == LACP_STATE_FULL
			    && (lacp_partner_state[i] & LACP_STATE_SYNC))
				lacp_scratch_mask |= PORT_BIT(i);
		}
		if (lacp_scratch_mask != lacp_members_last[lag]) {
			lacp_members_last[lag] = lacp_scratch_mask;
			port_lag_members_set(lag, lacp_scratch_mask);
		}
	}
}


void lacp_send(uint8_t port) __banked
{
	uint16_t saved_mgmt_vlan;

	memset((__xdata uint8_t *)LACP_O, 0, sizeof(struct lacpdu));
	LACP_O->dst[0] = 0x01; LACP_O->dst[1] = 0x80; LACP_O->dst[2] = 0xc2;
	LACP_O->dst[3] = 0x00; LACP_O->dst[4] = 0x00; LACP_O->dst[5] = LACP_DST5;
	memcpy(LACP_O->src, uip_ethaddr.addr, MAC_ADDR_LEN);

	LACP_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	LACP_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
	LACP_O->rtl_tag.reason = 0x00;
	LACP_O->rtl_tag.flags = HTONS(RTL_TAG_LEARN_DIS | RTL_TAG_KEEP);
	LACP_O->rtl_tag.pmask = HTONS(PORT_BIT(port));

	LACP_O->ethertype = HTONS(SLOW_PROTO_ETHERTYPE);
	LACP_O->subtype = SLOW_PROTO_SUBTYPE_LACP;
	LACP_O->version = LACP_VERSION;

	LACP_O->tlv_actor = LACP_TLV_ACTOR;
	LACP_O->actor_len = LACP_TLV_LEN_INFO;
	LACP_O->actor.sys_prio = HTONS(LACP_SYS_PRIO);
	memcpy(LACP_O->actor.sys, uip_ethaddr.addr, MAC_ADDR_LEN);
	LACP_O->actor.key = HTONS((uint16_t)(lacp_port_lag[port] + 1));
	LACP_O->actor.port_prio = HTONS(LACP_DEF_PORT_PRIO);
	LACP_O->actor.port = HTONS((uint16_t)port + 1);
	LACP_O->actor.state = lacp_actor_state[port];

	LACP_O->tlv_partner = LACP_TLV_PARTNER;
	LACP_O->partner_len = LACP_TLV_LEN_INFO;
	LACP_O->partner.sys_prio = HTONS(lacp_partner_sys_prio[port]);
	memcpy(LACP_O->partner.sys, lacp_partner_sys[port], MAC_ADDR_LEN);
	LACP_O->partner.key = HTONS(lacp_partner_key[port]);
	LACP_O->partner.port_prio = HTONS(lacp_partner_port_prio[port]);
	LACP_O->partner.port = HTONS(lacp_partner_port[port]);
	LACP_O->partner.state = lacp_partner_state[port];

	LACP_O->tlv_collector = LACP_TLV_COLLECTOR;
	LACP_O->collector_len = LACP_TLV_LEN_COLLECTOR;
	LACP_O->collector_max_delay = 0;
	LACP_O->tlv_terminator = LACP_TLV_TERMINATOR;
	LACP_O->terminator_len = 0x00;

	lacp_ntt[port] = 0;
	saved_mgmt_vlan = management_vlan;
	management_vlan = 0;
	uip_len = sizeof(struct lacpdu);
	tcpip_output();
	management_vlan = saved_mgmt_vlan;
}


void lacp_in(void) __banked
{
	__xdata uint8_t port, lag;

	lacp_rxbody = TRAP_RX_BODY();
	if (uip_len < lacp_rxbody + sizeof(struct lacpdu_in)) {
		uip_len = 0;
		return;
	}
	uip_len = 0;

	if (LACP_I->ethertype != HTONS(SLOW_PROTO_ETHERTYPE)
	    || LACP_I->subtype != SLOW_PROTO_SUBTYPE_LACP)
		return;

	port = ((uint8_t)HTONS(LACP_H->rtl_tag.pmask)) & 0x0f;
	if (port < machine.min_port || port > machine.max_port)
		return;

	lag = lacp_port_lag[port];
	if (lag == LACP_LAG_NONE)
		return;
	if (lacp_sys_eq(LACP_I->actor.sys, uip_ethaddr.addr))
		return;


#ifdef DEBUG
	print_string("LACP in, port "); print_byte(port);
	print_string(" partner_state "); print_byte(LACP_I->actor.state); write_char('\n');
#endif

	lacp_rx_count[port]++;

	memcpy(lacp_partner_sys[port], LACP_I->actor.sys, MAC_ADDR_LEN);
	lacp_scratch_mask = HTONS(LACP_I->actor.key);
	lacp_partner_key[port] = lacp_scratch_mask;
	lacp_scratch_mask = HTONS(LACP_I->actor.port);
	lacp_partner_port[port] = lacp_scratch_mask;
	lacp_scratch_mask = HTONS(LACP_I->actor.sys_prio);
	lacp_partner_sys_prio[port] = lacp_scratch_mask;
	lacp_scratch_mask = HTONS(LACP_I->actor.port_prio);
	lacp_partner_port_prio[port] = lacp_scratch_mask;
	lacp_partner_state[port]     = LACP_I->actor.state;

	lacp_rx_state[port] = LACP_RX_CURRENT;
	lacp_timeout[port] = LACP_SHORT_TIMEOUT;

	if (!lacp_agg_valid[lag]) {
		memcpy(lacp_agg_sys[lag], LACP_I->actor.sys, MAC_ADDR_LEN);
		lacp_agg_valid[lag] = 1;
	}

	if (lacp_mux_machine(port))
		lacp_ntt[port] = 1;

	if (LACP_I->partner.state != lacp_actor_state[port]
	    || HTONS(LACP_I->partner.port) != (uint16_t)port + 1)
		lacp_ntt[port] = 1;

	lacp_mux_update();
}



static void lacp_step(void)
{
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		if (lacp_port_lag[i] == LACP_LAG_NONE)
			continue;

		if (lacp_periodic[i]) {
			lacp_periodic[i]--;
		} else {
			lacp_periodic[i] = (lacp_partner_state[i] & LACP_STATE_TIMEOUT) ?
				LACP_FAST_PERIODIC : LACP_SLOW_PERIODIC;
			lacp_ntt[i] = 1;
		}

		if (lacp_timeout[i]) {
			if (!--lacp_timeout[i]) {
				lacp_rx_state[i] = LACP_RX_DEFAULTED;
				lacp_partner_state[i] = LACP_STATE_DEFAULTED;
				if (lacp_mux_machine(i))
					lacp_ntt[i] = 1;
			}
		}

		if (lacp_ntt[i])
			lacp_send(i);
	}

	for (uint8_t lag = 0; lag < LACP_NUM_LAGS; lag++) {
		uint8_t any_current = 0;

		if (!lacp_agg_valid[lag])
			continue;
		for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
			if (lacp_port_lag[i] == lag && lacp_rx_state[i] == LACP_RX_CURRENT
			    && lacp_sys_eq(lacp_partner_sys[i], lacp_agg_sys[lag])) {
				any_current = 1;
				break;
			}
		}
		if (!any_current)
			lacp_agg_valid[lag] = 0;
	}

	lacp_mux_update();
}


void lacp_timers(void) __banked
{
	uint8_t n = LACP_CATCH_UP;
	while (n-- && TICKS_DUE(lacp_tick_last, LACP_TICK_STEP)) {
		lacp_tick_last += LACP_TICK_STEP;
		lacp_step();
	}
}


static void lacp_port_init(uint8_t port)
{
	lacp_actor_state[port] = LACP_STATE_ACTIVITY | LACP_STATE_AGGREGATION |
		LACP_STATE_TIMEOUT;
	lacp_rx_state[port] = LACP_RX_INITIALIZE;
	lacp_partner_state[port] = LACP_STATE_DEFAULTED;
	memset(lacp_partner_sys[port], 0, MAC_ADDR_LEN);
	lacp_partner_key[port] = 0;
	lacp_partner_port[port] = 0;
	lacp_periodic[port] = LACP_FAST_PERIODIC;
	lacp_timeout[port] = 0;
	lacp_ntt[port] = 1;
	lacp_rx_count[port] = 0;
}

static void lacp_port_release(uint8_t port)
{
	lacp_actor_state[port] = 0;
	lacp_ntt[port] = 0;
	lacp_partner_state[port] = 0;
	lacp_rx_state[port] = LACP_RX_INITIALIZE;
	lacp_rx_count[port] = 0;
	memset(lacp_partner_sys[port], 0, MAC_ADDR_LEN);
}

static uint8_t lacp_any_lag(void)
{
	for (uint8_t l = 0; l < LACP_NUM_LAGS; l++)
		if (lacp_lag_ports[l])
			return 1;
	return 0;
}

static void lacp_engine_on(void)
{
	lacp_tick_last = (uint8_t)ticks;
	lacpEnabled = 1;
	trap_rma_set(RTL837X_RMA2_CONF, RTL837X_RMA_ACT_TRAP);
}

static void lacp_engine_off(void)
{
	trap_rma_set(RTL837X_RMA2_CONF, RTL837X_RMA_ACT_DROP);
	lacpEnabled = 0;
}

void lacp_init(void) __banked
{
	for (uint8_t i = 0; i < 10; i++) {
		lacp_port_lag[i] = LACP_LAG_NONE;
		lacp_actor_state[i] = 0;
		lacp_ntt[i] = 0;
	}
	for (uint8_t l = 0; l < LACP_NUM_LAGS; l++) {
		lacp_lag_ports[l] = 0;
		lacp_agg_valid[l] = 0;
		lacp_members_last[l] = 0;
	}
	lacpEnabled = 0;
}

void lacp_lag_set(uint8_t lag, __xdata uint16_t ports) __banked
{
	if (lag >= LACP_NUM_LAGS)
		return;
	if (ports && ports == lacp_lag_ports[lag])
		return;
	lacp_scratch_flag = lacp_any_lag();

	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		if (lacp_port_lag[i] == lag && !(ports & PORT_BIT(i))) {
			lacp_port_lag[i] = LACP_LAG_NONE;
			lacp_port_release(i);
		}
	}
	if (lacp_members_last[lag] || ports) {
		lacp_members_last[lag] = 0;
		port_lag_members_set(lag, 0);
	}
	lacp_agg_valid[lag] = 0;
	lacp_lag_ports[lag] = ports;

	if (ports && !lacp_scratch_flag)
		lacp_engine_on();

	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		if (!(ports & PORT_BIT(i)))
			continue;
		lacp_scratch_lag = lacp_port_lag[i];
		lacp_port_lag[i] = lag;
		if (lacp_scratch_lag != LACP_LAG_NONE && lacp_scratch_lag != lag) {
			lacp_scratch_mask = lacp_lag_ports[lacp_scratch_lag] & ~PORT_BIT(i);
			lacp_lag_ports[lacp_scratch_lag] = lacp_scratch_mask;
			if (!lacp_scratch_mask) {
				lacp_agg_valid[lacp_scratch_lag] = 0;
				if (lacp_members_last[lacp_scratch_lag]) {
					lacp_members_last[lacp_scratch_lag] = 0;
					port_lag_members_set(lacp_scratch_lag, 0);
				}
			}
		}
		lacp_port_init(i);
	}

	if (!lacp_any_lag() && lacp_scratch_flag)
		lacp_engine_off();

	lacp_mux_update();
}


void lacp_off(void) __banked
{
	for (uint8_t l = 0; l < LACP_NUM_LAGS; l++)
		if (lacp_lag_ports[l])
			lacp_lag_set(l, 0);
}


void lacp_show(void) __banked
{
	print_string("LACP "); print_string(lacpEnabled ? "on" : "off"); write_char('\n');
	for (uint8_t l = 0; l < LACP_NUM_LAGS; l++) {
		if (!lacp_lag_ports[l])
			continue;
		print_string("lag "); print_byte(l + 1);
		print_string(" ports "); print_short(lacp_lag_ports[l]);
		print_string(" aggregator ");
		if (lacp_agg_valid[l]) {
			for (uint8_t j = 0; j < MAC_ADDR_LEN; j++)
				print_byte(lacp_agg_sys[l][j]);
		} else {
			print_string("(none)");
		}
		print_string(" members "); print_short(lacp_members_last[l]); write_char('\n');
	}

	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		print_string("port "); print_byte(machine.log_to_phys_port[i]);
		print_string(" actor "); print_byte(lacp_actor_state[i]);
		print_string(" partner "); print_byte(lacp_partner_state[i]);
		print_string(" rxst "); print_byte(lacp_rx_state[i]);
		print_string(" rx "); print_short(lacp_rx_count[i]);
		print_string(" psys ");
		for (uint8_t j = 0; j < MAC_ADDR_LEN; j++)
			print_byte(lacp_partner_sys[i][j]);
		write_char('\n');
	}
}

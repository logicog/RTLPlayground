#include "rtl837x_sfr.h"
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_port.h"
#include "uip.h"
#include <stdint.h>
#include "phy.h"
#include "machine.h"
#include "rtl837x_stp.h"
#include "page_impl.h"
#include "debug.h"

#pragma codeseg BANK4
#pragma constseg BANK4

extern __xdata uint8_t outbuf[TCP_OUTBUF_SIZE];
extern __xdata uint16_t slen;

static __xdata uint32_t pi_u32;
static __xdata uint8_t pi_prio, pi_ext;
static __xdata uint8_t * __xdata pi_mac;


static void u32hex_html(void)
{
	__xdata uint8_t *b = (__xdata uint8_t *)&pi_u32;
	byte_to_html(b[3]);
	byte_to_html(b[2]);
	byte_to_html(b[1]);
	byte_to_html(b[0]);
}


static void bridge_to_html(void)
{
	byte_to_html(pi_prio);
	byte_to_html(pi_ext);
	for (uint8_t i = 0; i < 6; i++)
		byte_to_html(pi_mac[i]);
}


void send_stp(void) __banked
{
	static __xdata uint8_t i, j, st, dsg;

	dbg_string("send_stp called\n");
	slen = strtox(outbuf, HTTP_RESPONCE_JSON);

	string_to_html("{\"on\":");
	bool_to_html(stp_enabled);
	string_to_html(",\"rstp\":");
	bool_to_html(stp_rstp);
	string_to_html(",\"prio\":");
	itoa_html(stp_prio >> 4);
	string_to_html(",\"hello\":");
	itoa_html(stp_hello_s);
	string_to_html(",\"maxage\":");
	itoa_html(stp_maxage_s);
	string_to_html(",\"fwd\":");
	itoa_html(stp_fwddelay_s);
	string_to_html(",\"txhold\":");
	itoa_html(stp_txhold);
	string_to_html(",\"pcs\":");
	itoa_html(stp_pcost_short);
	string_to_html(",\"bh\":");
	itoa_html(stp_bpdu_filter);
	string_to_html(",\"tcs\":\"");
	pi_u32 = stp_tc_secs; u32hex_html();
	string_to_html("\",\"rMaxage\":");
	itoa_html(stp_root_port == 0xff ? stp_maxage_s : stp_root_maxage);
	string_to_html(",\"rFwd\":");
	itoa_html(stp_root_port == 0xff ? stp_fwddelay_s : stp_root_fwd);
	string_to_html(",\"rootPrio\":\"");
	byte_to_html(root_bridge.prio);
	byte_to_html(root_bridge.ext);
	string_to_html("\",\"rootMac\":\"");
	for (j = 0; j < 6; j++)
		byte_to_html(root_bridge.mac[j]);
	string_to_html("\",\"myMac\":\"");
	for (j = 0; j < 6; j++)
		byte_to_html(uip_ethaddr.addr[j]);
	string_to_html("\",\"cost\":\"");
	byte_to_html(root_bridge_cost >> 24);
	byte_to_html(root_bridge_cost >> 16);
	byte_to_html(root_bridge_cost >> 8);
	byte_to_html(root_bridge_cost);
	string_to_html("\",\"weRoot\":");
	bool_to_html(stp_root_port == 0xff ? 1 : 0);
	string_to_html(",\"tc\":\"");
	byte_to_html(stp_tc_count >> 8);
	byte_to_html(stp_tc_count);
	string_to_html("\",\"ports\":[");
	for (i = 0; i < STP_ENTITIES; i++) {
		j = stp_ent_id(i);
		if (!j)
			continue;
		string_to_html("{\"p\":");
		itoa_html(j);
		string_to_html(",\"st\":");
		st = stp_port_state(i);
		itoa_html(st);
		string_to_html(",\"role\":");
		itoa_html(stp_port_role(i));
		string_to_html(",\"f\":");
		itoa_html(stp_pflags[i]);
		string_to_html(",\"pc\":\"");
		pi_u32 = stp_pcost[i]; u32hex_html();
		if (i >= STP_LAG_BASE) {
			string_to_html("\",\"mbr\":\"");
			pi_u32 = stp_lag_mask[i - STP_LAG_BASE]; u32hex_html();
		}
		string_to_html("\",\"prio\":");
		itoa_html(stp_pprio[i]);
		string_to_html(",\"p2\":");
		itoa_html(stp_pp2p[i]);
		string_to_html(",\"lk\":");
		itoa_html((stp_link_prev >> i) & 1);
		string_to_html(",\"lg\":");
		itoa_html((stp_legacy >> i) & 1);
		dsg = stp_dpid[i] && stp_info_while[i];
		string_to_html(",\"db\":\"");
		if (dsg) {
			pi_prio = stp_dbridge[i].prio; pi_ext = stp_dbridge[i].ext;
			pi_mac = stp_dbridge[i].mac;
		} else {
			pi_prio = stp_prio; pi_ext = 0;
			pi_mac = uip_ethaddr.addr;
		}
		bridge_to_html();
		string_to_html("\",\"dp\":\"");
		st = i;
		if (!dsg && ((stp_backup >> i) & 1) && stp_loop_held[i]) {
			st = stp_loop_held[i] - 1;
			j = stp_ent_id(st);
		}
		byte_to_html(dsg ? (stp_dpid[i] >> 8) : stp_pprio[st]);
		byte_to_html(dsg ? stp_dpid[i] : j);
		string_to_html("\",\"dc\":\"");
		pi_u32 = dsg ? stp_dcost[i] : root_bridge_cost; u32hex_html();
		string_to_html("\"},");
	}
	slen -= 1; // remove comma
	string_to_html("]}");
}


void send_stp_counters(void) __banked
{
	uint8_t i, j;

	slen = strtox(outbuf, HTTP_RESPONCE_JSON);
	string_to_html("{\"on\":");
	bool_to_html(stp_enabled);
	string_to_html(",\"hz\":");
	itoa_html(STP_HZ);
	string_to_html(",\"tc\":\"");
	byte_to_html(stp_tc_count >> 8);
	byte_to_html(stp_tc_count);
	string_to_html("\",\"ports\":[");
	for (i = 0; i < STP_ENTITIES; i++) {
		j = stp_ent_id(i);
		if (!j)
			continue;
		string_to_html("{\"p\":");
		itoa_html(j);
		string_to_html(",\"c\":\"");
		for (j = 0; j < STP_CNT_N; j++) {
			pi_u32 = stp_cnt[j][i]; u32hex_html();
		}
		byte_to_html(stp_bpdu_age[i] >> 8);
		byte_to_html(stp_bpdu_age[i]);
		string_to_html("\"},");
	}
	slen -= 1;
	string_to_html("]}");
}

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

	slen += strtox(outbuf + slen, "{\"on\":");
	bool_to_html(stp_enabled);
	slen += strtox(outbuf + slen, ",\"rstp\":");
	bool_to_html(stp_rstp);
	slen += strtox(outbuf + slen, ",\"prio\":");
	itoa_html(stp_prio >> 4);
	slen += strtox(outbuf + slen, ",\"hello\":");
	itoa_html(stp_hello_s);
	slen += strtox(outbuf + slen, ",\"maxage\":");
	itoa_html(stp_maxage_s);
	slen += strtox(outbuf + slen, ",\"fwd\":");
	itoa_html(stp_fwddelay_s);
	slen += strtox(outbuf + slen, ",\"txhold\":");
	itoa_html(stp_txhold);
	slen += strtox(outbuf + slen, ",\"pcs\":");
	itoa_html(stp_pcost_short);
	slen += strtox(outbuf + slen, ",\"bh\":");
	itoa_html(stp_bpdu_filter);
	slen += strtox(outbuf + slen, ",\"tcs\":\"");
	pi_u32 = stp_tc_secs; u32hex_html();
	slen += strtox(outbuf + slen, "\",\"rMaxage\":");
	itoa_html(stp_root_port == 0xff ? stp_maxage_s : stp_root_maxage);
	slen += strtox(outbuf + slen, ",\"rFwd\":");
	itoa_html(stp_root_port == 0xff ? stp_fwddelay_s : stp_root_fwd);
	slen += strtox(outbuf + slen, ",\"rootPrio\":\"");
	byte_to_html(root_bridge.prio);
	byte_to_html(root_bridge.ext);
	slen += strtox(outbuf + slen, "\",\"rootMac\":\"");
	for (j = 0; j < 6; j++)
		byte_to_html(root_bridge.mac[j]);
	slen += strtox(outbuf + slen, "\",\"myMac\":\"");
	for (j = 0; j < 6; j++)
		byte_to_html(uip_ethaddr.addr[j]);
	slen += strtox(outbuf + slen, "\",\"cost\":\"");
	byte_to_html(root_bridge_cost >> 24);
	byte_to_html(root_bridge_cost >> 16);
	byte_to_html(root_bridge_cost >> 8);
	byte_to_html(root_bridge_cost);
	slen += strtox(outbuf + slen, "\",\"weRoot\":");
	bool_to_html(stp_root_port == 0xff ? 1 : 0);
	slen += strtox(outbuf + slen, ",\"tc\":\"");
	byte_to_html(stp_tc_count >> 8);
	byte_to_html(stp_tc_count);
	slen += strtox(outbuf + slen, "\",\"ports\":[");
	for (i = 0; i < STP_ENTITIES; i++) {
		j = stp_ent_id(i);
		if (!j)
			continue;
		slen += strtox(outbuf + slen, "{\"p\":");
		itoa_html(j);
		slen += strtox(outbuf + slen, ",\"st\":");
		st = stp_port_state(i);
		itoa_html(st);
		slen += strtox(outbuf + slen, ",\"role\":");
		itoa_html(stp_port_role(i));
		slen += strtox(outbuf + slen, ",\"f\":");
		itoa_html(stp_pflags[i]);
		slen += strtox(outbuf + slen, ",\"pc\":\"");
		pi_u32 = stp_pcost[i]; u32hex_html();
		slen += strtox(outbuf + slen, "\",\"prio\":");
		itoa_html(stp_pprio[i]);
		slen += strtox(outbuf + slen, ",\"p2\":");
		itoa_html(stp_pp2p[i]);
		slen += strtox(outbuf + slen, ",\"lk\":");
		itoa_html((stp_link_prev >> i) & 1);
		slen += strtox(outbuf + slen, ",\"lg\":");
		itoa_html((stp_legacy >> i) & 1);
		dsg = stp_dpid[i] && stp_info_while[i];
		slen += strtox(outbuf + slen, ",\"db\":\"");
		if (dsg) {
			pi_prio = stp_dbridge[i].prio; pi_ext = stp_dbridge[i].ext;
			pi_mac = stp_dbridge[i].mac;
		} else {
			pi_prio = stp_prio; pi_ext = 0;
			pi_mac = uip_ethaddr.addr;
		}
		bridge_to_html();
		slen += strtox(outbuf + slen, "\",\"dp\":\"");
		byte_to_html(dsg ? (stp_dpid[i] >> 8) : stp_pprio[i]);
		byte_to_html(dsg ? stp_dpid[i] : j);
		slen += strtox(outbuf + slen, "\",\"dc\":\"");
		pi_u32 = dsg ? stp_dcost[i] : root_bridge_cost; u32hex_html();
		slen += strtox(outbuf + slen, "\"},");
	}
	slen -= 1; // remove comma
	slen += strtox(outbuf + slen, "]}");
}


void send_stp_counters(void) __banked
{
	uint8_t i, j;

	slen = strtox(outbuf, HTTP_RESPONCE_JSON);
	slen += strtox(outbuf + slen, "{\"on\":");
	bool_to_html(stp_enabled);
	slen += strtox(outbuf + slen, ",\"hz\":");
	itoa_html(STP_HZ);
	slen += strtox(outbuf + slen, ",\"tc\":\"");
	byte_to_html(stp_tc_count >> 8);
	byte_to_html(stp_tc_count);
	slen += strtox(outbuf + slen, "\",\"ports\":[");
	for (i = 0; i < STP_ENTITIES; i++) {
		j = stp_ent_id(i);
		if (!j)
			continue;
		slen += strtox(outbuf + slen, "{\"p\":");
		itoa_html(j);
		slen += strtox(outbuf + slen, ",\"c\":\"");
		for (j = 0; j < STP_CNT_N; j++) {
			pi_u32 = stp_cnt[j][i]; u32hex_html();
		}
		byte_to_html(stp_bpdu_age[i] >> 8);
		byte_to_html(stp_bpdu_age[i]);
		slen += strtox(outbuf + slen, "\"},");
	}
	slen -= 1;
	slen += strtox(outbuf + slen, "]}");
}

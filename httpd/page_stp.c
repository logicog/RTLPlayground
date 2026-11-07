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

extern __code const struct machine machine;
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
	slen = strtox(outbuf, HTTP_RESPONSE_JSON);

	str_code_to_html("{" JSONQUOTE("on") ":");
	bool_to_json(stp_enabled);
	str_code_to_html(","  JSONQUOTE("type") ":");
	if(stp_rstp) {
		str_code_to_html(JSONQUOTE("rstp"));
	} else {
		str_code_to_html(JSONQUOTE("stp"));
	}
	str_code_to_html("," JSONQUOTE("prio") ":");
	itoa_html(stp_prio >> 4);
	str_code_to_html("," JSONQUOTE("hello") ":");
	itoa_html(stp_hello_s);
	str_code_to_html("," JSONQUOTE("maxage") ":");
	itoa_html(stp_maxage_s);
	str_code_to_html("," JSONQUOTE("fwd")":");
	itoa_html(stp_fwddelay_s);
	str_code_to_html("," JSONQUOTE("txhold") ":");
	itoa_html(stp_txhold);
	str_code_to_html("," JSONQUOTE("pcs") ":");
	if(stp_pcost_short) {
		str_code_to_html(JSONQUOTE("short"));
	} else {
		str_code_to_html(JSONQUOTE("long"));
	}
	str_code_to_html("," JSONQUOTE("bh") ":");
	if(stp_bpdu_filter) {
		str_code_to_html(JSONQUOTE("filter"));
	} else {
		str_code_to_html(JSONQUOTE("flood"));
	}
	str_code_to_html("," JSONQUOTE("tcs") ":\"");
	pi_u32 = stp_tc_secs; u32hex_html();
	str_code_to_html("\"," JSONQUOTE("rMaxage") ":");
	itoa_html(stp_root_port == 0xff ? stp_maxage_s : stp_root_maxage);
	str_code_to_html("," JSONQUOTE("rFwd") ":");
	itoa_html(stp_root_port == 0xff ? stp_fwddelay_s : stp_root_fwd);
	str_code_to_html("," JSONQUOTE("rootPrio") ":\"");
	byte_to_html(root_bridge.prio);
	byte_to_html(root_bridge.ext);
	str_code_to_html("\"," JSONQUOTE("rootMac") ":\"");
	for (j = 0; j < 6; j++)
		byte_to_html(root_bridge.mac[j]);
	str_code_to_html("\"," JSONQUOTE("myMac") ":\"");
	for (j = 0; j < 6; j++)
		byte_to_html(uip_ethaddr.addr[j]);
	str_code_to_html("\"," JSONQUOTE("cost") ":\"");
	byte_to_html(root_bridge_cost >> 24);
	byte_to_html(root_bridge_cost >> 16);
	byte_to_html(root_bridge_cost >> 8);
	byte_to_html(root_bridge_cost);
	str_code_to_html("\"," JSONQUOTE("weRoot") ":");
	bool_to_json(stp_root_port == 0xff ? 1 : 0);
	str_code_to_html("," JSONQUOTE("tc") ":\"");
	byte_to_html(stp_tc_count >> 8);
	byte_to_html(stp_tc_count);
	str_code_to_html("\"," JSONQUOTE("ports") ":[");
	for (i = 0; i < STP_ENTITIES; i++) {
		j = stp_ent_id(i);
		if (!j)
			continue;
		str_code_to_html("{" JSONQUOTE("p") ":");
		itoa_html(j);
		str_code_to_html("," JSONQUOTE("st") ":");
		st = stp_port_state(i);
		itoa_html(st);
		str_code_to_html("," JSONQUOTE("role") ":");
		itoa_html(stp_port_role(i));
		str_code_to_html("," JSONQUOTE("f") ":");
		itoa_html(stp_pflags[i]);
		str_code_to_html("," JSONQUOTE("pc") ":\"");
		pi_u32 = stp_pcost[i]; u32hex_html();
		str_code_to_html("\"");
		if (i >= STP_LAG_BASE) {
			str_code_to_html("," JSONQUOTE("mbr") ":[");
			for (uint8_t j = MACHINE_MIN_PORT; j <= MACHINE_MAX_PORT; j++) {
				if(stp_lag_mask[i - STP_LAG_BASE] & (1<<j)) {
					itoa_html(machine.log_to_phys_port[j]);
					str_code_to_html(",");
				}
			}
			if(outbuf[slen-1] == ',')
				slen--;
			str_code_to_html("]");
		}
		
		str_code_to_html("," JSONQUOTE("prio") ":");
		itoa_html(stp_pprio[i]);
		str_code_to_html("," JSONQUOTE("p2") ":");
		itoa_html(stp_pp2p[i]);
		str_code_to_html("," JSONQUOTE("lk") ":");
		itoa_html((stp_link_prev >> i) & 1);
		str_code_to_html("," JSONQUOTE("lg") ":");
		itoa_html((stp_legacy >> i) & 1);
		dsg = stp_dpid[i] && stp_info_while[i];
		str_code_to_html("," JSONQUOTE("db") ":\"");
		if (dsg) {
			pi_prio = stp_dbridge[i].prio; pi_ext = stp_dbridge[i].ext;
			pi_mac = stp_dbridge[i].mac;
		} else {
			pi_prio = stp_prio; pi_ext = 0;
			pi_mac = uip_ethaddr.addr;
		}
		bridge_to_html();
		str_code_to_html("\"," JSONQUOTE("dp") ":\"");
		st = i;
		if (!dsg && ((stp_backup >> i) & 1) && stp_loop_held[i]) {
			st = stp_loop_held[i] - 1;
			j = stp_ent_id(st);
		}
		byte_to_html(dsg ? (stp_dpid[i] >> 8) : stp_pprio[st]);
		byte_to_html(dsg ? stp_dpid[i] : j);
		str_code_to_html("\"," JSONQUOTE("dc") ":\"");
		pi_u32 = dsg ? stp_dcost[i] : root_bridge_cost; u32hex_html();
		str_code_to_html("\"},");
	}
	slen -= 1; // remove comma
	char_to_html(']');
	char_to_html('}');
}


void send_stp_counters(void) __banked
{
	uint8_t i, j;

	slen = strtox(outbuf, HTTP_RESPONSE_JSON);
	str_code_to_html("{" JSONQUOTE("on") ":");
	bool_to_json(stp_enabled);
	str_code_to_html("," JSONQUOTE("hz") ":");
	itoa_html(STP_HZ);
	str_code_to_html("," JSONQUOTE("tc") ":\"");
	byte_to_html(stp_tc_count >> 8);
	byte_to_html(stp_tc_count);
	str_code_to_html("\"," JSONQUOTE("ports") ":[");
	for (i = 0; i < STP_ENTITIES; i++) {
		j = stp_ent_id(i);
		if (!j)
			continue;
		str_code_to_html("{" JSONQUOTE("p") ":");
		itoa_html(j);
		str_code_to_html("," JSONQUOTE("c") ":\"");
		for (j = 0; j < STP_CNT_N; j++) {
			pi_u32 = stp_cnt[j][i]; u32hex_html();
		}
		byte_to_html(stp_bpdu_age[i] >> 8);
		byte_to_html(stp_bpdu_age[i]);
		str_code_to_html("\"},");
	}
	slen -= 1;
	char_to_html(']');
	char_to_html('}');
}

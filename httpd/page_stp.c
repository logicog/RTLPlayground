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

static __code const uint8_t stp_json_hdr[] = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\n\r\n";

static void stp_bool_html(char c)
{
	outbuf[slen++] = c ? '1' : '0';
}

static void stp_byte_html(uint8_t val)
{
	outbuf[slen++] = itohex(val >> 4);
	outbuf[slen++] = itohex(val);
}

static void stp_itoa_html(uint8_t v)
{
	uint8_t t = v / 100;
	uint8_t print_zeros = t;
	if (print_zeros)
		outbuf[slen++] = '0' + t;
	t = (v / 10) % 10;
	print_zeros |= t;
	if (print_zeros)
		outbuf[slen++] = '0' + t;
	outbuf[slen++] = '0' + (v % 10);
}


static __xdata uint32_t pi_u32;
static __xdata uint8_t pi_prio, pi_ext;
static __xdata uint8_t * __xdata pi_mac;


static void u32hex_html(void)
{
	__xdata uint8_t *b = (__xdata uint8_t *)&pi_u32;
	stp_byte_html(b[3]);
	stp_byte_html(b[2]);
	stp_byte_html(b[1]);
	stp_byte_html(b[0]);
}


static void bridge_to_html(void)
{
	stp_byte_html(pi_prio);
	stp_byte_html(pi_ext);
	for (uint8_t i = 0; i < 6; i++)
		stp_byte_html(pi_mac[i]);
}


void send_stp(void) __banked
{
	static __xdata uint8_t i, j, st, dsg;

	dbg_string("send_stp called\n");
	slen = strtox(outbuf, stp_json_hdr);

	slen += strtox(outbuf + slen, "{\"on\":");
	stp_bool_html(stp_enabled);
	slen += strtox(outbuf + slen, ",\"rstp\":");
	stp_itoa_html(stp_rstp);
	slen += strtox(outbuf + slen, ",\"prio\":");
	stp_itoa_html(stp_prio >> 4);
	slen += strtox(outbuf + slen, ",\"hello\":");
	stp_itoa_html(stp_hello_s);
	slen += strtox(outbuf + slen, ",\"maxage\":");
	stp_itoa_html(stp_maxage_s);
	slen += strtox(outbuf + slen, ",\"fwd\":");
	stp_itoa_html(stp_fwddelay_s);
	slen += strtox(outbuf + slen, ",\"txhold\":");
	stp_itoa_html(stp_txhold);
	slen += strtox(outbuf + slen, ",\"pcs\":");
	stp_itoa_html(stp_pcost_short);
	slen += strtox(outbuf + slen, ",\"bh\":");
	stp_itoa_html(stp_bpdu_filter);
	slen += strtox(outbuf + slen, ",\"tcs\":\"");
	pi_u32 = stp_tc_secs; u32hex_html();
	slen += strtox(outbuf + slen, "\",\"rMaxage\":");
	stp_itoa_html(stp_root_port == 0xff ? stp_maxage_s : stp_root_maxage);
	slen += strtox(outbuf + slen, ",\"rFwd\":");
	stp_itoa_html(stp_root_port == 0xff ? stp_fwddelay_s : stp_root_fwd);
	slen += strtox(outbuf + slen, ",\"rootPrio\":\"");
	stp_byte_html(stp_rv[0].root.prio);
	stp_byte_html(stp_rv[0].root.ext);
	slen += strtox(outbuf + slen, "\",\"rootMac\":\"");
	for (j = 0; j < 6; j++)
		stp_byte_html(stp_rv[0].root.mac[j]);
	slen += strtox(outbuf + slen, "\",\"myMac\":\"");
	for (j = 0; j < 6; j++)
		stp_byte_html(uip_ethaddr.addr[j]);
	slen += strtox(outbuf + slen, "\",\"cost\":\"");
	for (j = 0; j < 4; j++)
		stp_byte_html(stp_rv[0].ext[j]);
	slen += strtox(outbuf + slen, "\",\"weRoot\":");
	stp_bool_html(stp_root_port == 0xff ? 1 : 0);
	slen += strtox(outbuf + slen, ",\"tc\":\"");
	stp_byte_html(stp_tc_count >> 8);
	stp_byte_html(stp_tc_count);
	slen += strtox(outbuf + slen, "\",\"ports\":[");
	for (i = 0; i < STP_ENTITIES; i++) {
		j = stp_ent_id(i);
		if (!j)
			continue;
		slen += strtox(outbuf + slen, "{\"p\":");
		stp_itoa_html(j);
		slen += strtox(outbuf + slen, ",\"st\":");
		st = stp_port_state(i);
		stp_itoa_html(st);
		slen += strtox(outbuf + slen, ",\"role\":");
		stp_itoa_html(stp_port_role(i));
		slen += strtox(outbuf + slen, ",\"f\":");
		stp_itoa_html(stp_pflags[i]);
		slen += strtox(outbuf + slen, ",\"pc\":\"");
		pi_u32 = stp_pcost[i]; u32hex_html();
		slen += strtox(outbuf + slen, "\",\"prio\":");
		stp_itoa_html(stp_pprio[i]);
		slen += strtox(outbuf + slen, ",\"p2\":");
		stp_itoa_html(stp_pp2p[i]);
		slen += strtox(outbuf + slen, ",\"lk\":");
		stp_itoa_html((stp_link_prev >> i) & 1);
		slen += strtox(outbuf + slen, ",\"lg\":");
		stp_itoa_html((stp_legacy >> i) & 1);
		dsg = stp_info_while[i] != 0;
		slen += strtox(outbuf + slen, ",\"db\":\"");
		if (dsg) {
			pi_prio = stp_pv[i].dbr.prio; pi_ext = stp_pv[i].dbr.ext;
			pi_mac = stp_pv[i].dbr.mac;
		} else {
			pi_prio = stp_prio; pi_ext = 0;
			pi_mac = uip_ethaddr.addr;
		}
		bridge_to_html();
		slen += strtox(outbuf + slen, "\",\"dp\":\"");
		stp_byte_html(dsg ? stp_pv[i].dpid[0] : stp_pprio[i]);
		stp_byte_html(dsg ? stp_pv[i].dpid[1] : (i + 1));
		slen += strtox(outbuf + slen, "\",\"dc\":\"");
		pi_mac = dsg ? stp_pv[i].ext : stp_rv[0].ext;
		for (j = 0; j < 4; j++)
			stp_byte_html(pi_mac[j]);
		slen += strtox(outbuf + slen, "\"},");
	}
	slen -= 1; // remove comma
	slen += strtox(outbuf + slen, "]}");
}


void send_stp_counters(void) __banked
{
	uint8_t i, j;

	slen = strtox(outbuf, stp_json_hdr);
	slen += strtox(outbuf + slen, "{\"on\":");
	stp_bool_html(stp_enabled);
	slen += strtox(outbuf + slen, ",\"hz\":");
	stp_itoa_html(STP_HZ);
	slen += strtox(outbuf + slen, ",\"tc\":\"");
	stp_byte_html(stp_tc_count >> 8);
	stp_byte_html(stp_tc_count);
	slen += strtox(outbuf + slen, "\",\"ports\":[");
	for (i = 0; i < STP_ENTITIES; i++) {
		j = stp_ent_id(i);
		if (!j)
			continue;
		slen += strtox(outbuf + slen, "{\"p\":");
		stp_itoa_html(j);
		slen += strtox(outbuf + slen, ",\"c\":\"");
		for (j = 0; j < STP_CNT_N; j++) {
			pi_u32 = stp_cnt[j][i]; u32hex_html();
		}
		stp_byte_html(stp_bpdu_age[i] >> 8);
		stp_byte_html(stp_bpdu_age[i]);
		slen += strtox(outbuf + slen, "\"},");
	}
	slen -= 1;
	slen += strtox(outbuf + slen, "]}");
}

#ifndef _RTL837X_STP_H_
#define _RTL837X_STP_H_

#include <stdint.h>
void stp_in(void) __banked;
void stp_setup(void) __banked;
void stp_timers(void) __banked;
void stp_off(void) __banked;
void stp_parse(void) __banked __reentrant;	/* "stp ..." CLI handler (cmd_parser delegates here) */
void stp_defaults(void) __banked;	/* boot init: 802.1D/w default configuration */

/* Tick rate of stp_timers(), also used by the web UI. */
#define STP_HZ 50

/* Bridge identifier as carried in a BPDU (priority, extension, MAC). */
struct bridge {
	uint8_t prio;
	uint8_t ext;
	uint8_t mac[6];
};

/* ---- Configuration (defaults per 802.1D-2004/802.1w, set in stp_defaults) --- */
extern __xdata uint8_t  stpEnabled;
extern __xdata uint8_t  stp_prio;	/* bridge priority, high byte: 0x80 = 32768; CLI takes 0-15 (steps of 4096) */
extern __xdata uint8_t  stp_hello_s;	/* hello time, 1-10 s (default 2)   */
extern __xdata uint8_t  stp_maxage_s;	/* max age, 6-40 s (default 20)     */
extern __xdata uint8_t  stp_fwddelay_s;	/* forward delay, 4-30 s (default 15); our listen period */
extern __xdata uint8_t  stp_rstp;	/* 1 = RSTP BPDUs (v2), 0 = STP-compatible Config BPDUs (v0) */
extern __xdata uint8_t  stp_txhold;	/* max BPDUs per port per second (default 6) */

/* Per-port config/status flags (stp_pflags[]) */
#define STP_PF_ENABLED	0x01	/* port participates in STP (default on)     */
#define STP_PF_ADMEDGE	0x02	/* admin edge: forwarding immediately        */
#define STP_PF_AUTOEDGE	0x04	/* auto edge: forward after 3 s without BPDU */
#define STP_PF_BPDUGUARD 0x08	/* disable port if a BPDU arrives            */
#define STP_PF_ROOTGUARD 0x10	/* never accept a better root on this port   */
#define STP_PF_FILTER	0x20	/* neither send nor accept BPDUs             */
#define STP_PF_OPEREDGE	0x40	/* runtime: port went forwarding as an edge  */
#define STP_PF_TRIPPED	0x80	/* runtime: disabled by BPDU guard           */

extern __xdata uint8_t  stp_pflags[10];
extern __xdata uint32_t stp_pcost[10];	/* path cost; 0 = auto (20000)      */
extern __xdata uint8_t  stp_pprio[10];
extern __xdata uint8_t  stp_pp2p[10];	/* admin point-to-point: 0 auto, 1 on, 2 off */

/* Last-heard designated info per port (from received BPDUs); consult
 * stp_bpdu_age to decide whether it is still current. */
extern __xdata struct bridge stp_dbridge[10];
extern __xdata uint16_t stp_dpid[10];
extern __xdata uint32_t stp_dcost[10];
extern __xdata uint16_t stp_bpdu_age[10];	/* ticks since a BPDU was heard */	/* port priority (default 0x80)     */

/* ---- Status, exposed read-only for the web UI (send_stp) ---- */
extern __xdata struct bridge root_bridge;
extern __xdata uint32_t root_bridge_cost;	/* our path cost to the root (0 if we are root) */
extern __xdata uint8_t  stp_root_port;		/* logical port towards the root; 0xff = we are root */
extern __xdata uint16_t stp_tc_count;		/* topology change counter (diagnostics) */

#endif

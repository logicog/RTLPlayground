#ifndef _RTL837X_STP_H_
#define _RTL837X_STP_H_

#include <stdint.h>
void stp_in(void) __banked;
void stp_setup(void) __banked;
void stp_timers(void) __banked;
void stp_off(void) __banked;
void stp_parse(void) __banked __reentrant;
void stp_defaults(void) __banked;
void stp_status(void) __banked;
void stp_port_admin(uint8_t port, uint8_t on) __banked;
void stp_prio_apply(void) __banked;
void stp_port_mcheck(uint8_t port) __banked;
uint8_t stp_port_role(uint8_t port) __banked;
uint8_t stp_port_state(uint8_t port) __banked;
uint8_t stp_ent_id(uint8_t e) __banked;
void stp_lag_map(void) __banked;
void stp_counters_clear(void) __banked __reentrant;

/* Tick rate of stp_timers(), also used by the web UI. */
#define STP_HZ 50

#define STP_PORTS	(CPU_PORT + 1)
#define STP_LAG_BASE	STP_PORTS
#define STP_LAG_COUNT	4
#define STP_ENTITIES	(STP_LAG_BASE + STP_LAG_COUNT)

/* Bridge identifier as carried in a BPDU (priority, extension, MAC). */
struct bridge {
	uint8_t prio;
	uint8_t ext;
	uint8_t mac[6];
};

extern __xdata uint8_t  stp_prio;
extern __xdata uint8_t  stp_hello_s;
extern __xdata uint8_t  stp_maxage_s;
extern __xdata uint8_t  stp_fwddelay_s;
extern __xdata uint8_t  stp_rstp;
extern __xdata uint8_t  stp_txhold;

/* Per-port config/status flags (stp_pflags[]) */
#define STP_PF_ENABLED	0x01	/* port participates in STP (default on)     */
#define STP_PF_ADMEDGE	0x02	/* admin edge: forwarding immediately        */
#define STP_PF_AUTOEDGE	0x04	/* auto edge: forward after 3 s without BPDU */
#define STP_PF_BPDUGUARD 0x08	/* disable port if a BPDU arrives            */
#define STP_PF_ROOTGUARD 0x10	/* never accept a better root on this port   */
#define STP_PF_FILTER	0x20	/* neither send nor accept BPDUs             */
#define STP_PF_OPEREDGE	0x40	/* runtime: port went forwarding as an edge  */
#define STP_PF_TRIPPED	0x80	/* runtime: disabled by BPDU guard           */

extern __xdata uint8_t  stp_pflags[STP_ENTITIES];
extern __xdata uint32_t stp_pcost[STP_ENTITIES];
extern __xdata uint8_t  stp_pprio[STP_ENTITIES];
extern __xdata uint8_t  stp_pp2p[STP_ENTITIES];
extern __xdata uint8_t  stp_ent_of[STP_PORTS];
extern __xdata uint16_t stp_lag_mask[STP_LAG_COUNT];

extern __xdata struct bridge stp_dbridge[STP_ENTITIES];
extern __xdata uint16_t stp_dpid[STP_ENTITIES];
extern __xdata uint32_t stp_dcost[STP_ENTITIES];
extern __xdata uint16_t stp_bpdu_age[STP_ENTITIES];
#define STP_CNT_RX	0	/* BPDUs taken in on an STP port          */
#define STP_CNT_TX	1	/* BPDUs put on the wire                  */
#define STP_CNT_TCRX	2	/* BPDUs received with the TC flag, TCNs  */
#define STP_CNT_TCTX	3	/* BPDUs sent with the TC flag, TCNs      */
#define STP_CNT_N	4
extern __xdata uint32_t stp_cnt[STP_CNT_N][STP_ENTITIES];
extern __xdata uint16_t stp_info_while[STP_ENTITIES];

extern __xdata struct bridge root_bridge;
extern __xdata uint32_t root_bridge_cost;
extern __xdata uint8_t  stp_root_port;
extern __xdata uint8_t  stp_root_maxage;
extern __xdata uint8_t  stp_root_fwd;
extern __xdata uint16_t stp_link_prev;
extern __xdata uint16_t stp_alt;
extern __xdata uint16_t stp_backup;
extern __xdata uint16_t stp_legacy;
extern __xdata uint16_t stp_rrwhile[STP_ENTITIES];
extern __xdata uint16_t stp_rbwhile[STP_ENTITIES];
extern __xdata uint16_t stp_tc_count;
extern __xdata uint32_t stp_tc_secs;
extern __xdata uint8_t  stp_pcost_short;
extern __xdata uint8_t  stp_bpdu_filter;

#endif

#ifndef _RTL837X_STP_H_
#define _RTL837X_STP_H_

#include <stdint.h>
void stp_in(void) __banked;
void stp_setup(void) __banked;
void stp_timers(void) __banked;
void stp_off(void) __banked;
void stp_parse(void) __banked __reentrant;
void stp_defaults(void) __banked;

/* Tick rate of stp_timers(), also used by the web UI. */
#define STP_HZ 50

#define STP_LAG_BASE	10
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
extern __xdata uint32_t stp_pcost[STP_ENTITIES];	/* path cost; 0 = auto (20000)      */
extern __xdata uint8_t  stp_pprio[STP_ENTITIES];
extern __xdata uint8_t  stp_pp2p[STP_ENTITIES];
extern __xdata uint8_t  stp_ent_of[10];		/* the STP entity a port answers to: itself, or its lag */
extern __xdata uint16_t stp_lag_mask[STP_LAG_COUNT];	/* member ports of each lag; 0 = not part of STP */

/* Last-heard designated info per port (from received BPDUs); consult
 * stp_bpdu_age to decide whether it is still current. */
extern __xdata struct bridge stp_dbridge[STP_ENTITIES];
extern __xdata uint16_t stp_dpid[STP_ENTITIES];
extern __xdata uint32_t stp_dcost[STP_ENTITIES];
extern __xdata uint16_t stp_bpdu_age[STP_ENTITIES];	/* ticks since a BPDU was heard */

extern __xdata struct bridge root_bridge;
extern __xdata uint32_t root_bridge_cost;
extern __xdata uint8_t  stp_root_port;
extern __xdata uint16_t stp_tc_count;

#endif

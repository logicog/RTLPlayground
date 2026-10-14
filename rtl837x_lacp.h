#ifndef _RTL837X_LACP_H_
#define _RTL837X_LACP_H_

/*
 * LACP (IEEE 802.3ad Clause 43) for the RTL837x platform.
 * This code is in the Public Domain.
 */

#include <stdint.h>
#include "rtl837x_common.h"

void lacp_init(void) __banked;
void lacp_in(void) __banked;
void lacp_timers(void) __banked;
void lacp_off(void) __banked;
void lacp_show(void) __banked;
void lacp_lag_set(uint8_t lag, __xdata uint16_t ports) __banked;

#define LACP_NUM_LAGS		4
#define LACP_LAG_NONE		0xff

extern __xdata uint8_t  lacpEnabled;
extern __xdata uint8_t  lacp_actor_state[10];
extern __xdata uint8_t  lacp_partner_state[10];
extern __xdata uint8_t  lacp_rx_state[10];
extern __xdata uint8_t  lacp_partner_sys[10][MAC_ADDR_LEN];
extern __xdata uint16_t lacp_rx_count[10];
extern __xdata uint8_t  lacp_port_lag[10];
extern __xdata uint16_t lacp_lag_ports[LACP_NUM_LAGS];
extern __xdata uint8_t  lacp_agg_sys[LACP_NUM_LAGS][MAC_ADDR_LEN];
extern __xdata uint8_t  lacp_agg_valid[LACP_NUM_LAGS];
extern __xdata uint16_t lacp_members_last[LACP_NUM_LAGS];

#define SLOW_PROTO_ETHERTYPE	0x8809
#define SLOW_PROTO_SUBTYPE_LACP	0x01
#define LACP_VERSION		0x01
#define LACP_DST5		0x02

#define LACP_TLV_ACTOR		0x01
#define LACP_TLV_PARTNER	0x02
#define LACP_TLV_COLLECTOR	0x03
#define LACP_TLV_TERMINATOR	0x00
#define LACP_TLV_LEN_INFO	0x14
#define LACP_TLV_LEN_COLLECTOR	0x10

#define LACP_SYS_PRIO		0xffff
#define LACP_DEF_PORT_PRIO	0x00ff

#define LACP_STATE_ACTIVITY	0x01	/* active LACP, sends periodically */
#define LACP_STATE_TIMEOUT	0x02	/* short timeout requested */
#define LACP_STATE_AGGREGATION	0x04	/* link can be aggregated */
#define LACP_STATE_SYNC		0x08	/* in sync with the partner */
#define LACP_STATE_COLLECTING	0x10
#define LACP_STATE_DISTRIBUTING	0x20
#define LACP_STATE_DEFAULTED	0x40	/* partner information is the default */
#define LACP_STATE_EXPIRED	0x80

#define LACP_STATE_FULL		(LACP_STATE_SYNC | LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING)

#define LACP_FAST_PERIODIC	50
#define LACP_SLOW_PERIODIC	1500
#define LACP_SHORT_TIMEOUT	200

#define LACP_RX_INITIALIZE	0
#define LACP_RX_PORT_DISABLED	1
#define LACP_RX_EXPIRED		2
#define LACP_RX_DEFAULTED	3
#define LACP_RX_CURRENT		4

#endif

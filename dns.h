#ifndef _DNS_H_
#define _DNS_H_

#include <stdint.h>

#define DNS_NAME_LEN	48

#define DNS_IDLE	0
#define DNS_PENDING	1
#define DNS_DONE	2
#define DNS_FAILED	3

struct uip_udp_conn;

struct dns_state {
	uint8_t server[2][4];	/* configured servers, 0.0.0.0 = unused */
	char name[DNS_NAME_LEN];	/* name of the current or last lookup */
	uint8_t addr[4];	/* address found by the last lookup */
	uint8_t status;		/* DNS_* */
	uint8_t verbose;	/* print the result of the lookup */
	uint8_t tries;		/* queries sent for the current lookup */
	uint8_t send;		/* a query is due on the connection */
	uint16_t id;
	uint32_t sent;		/* ticks when the last query went out */
	struct uip_udp_conn *conn;
};

extern __xdata struct dns_state dns_state;

void dns_init(void) __banked;
void dns_lookup(void) __banked;
void dns_show(void) __banked;
void dns_callback(uint16_t lport) __banked __reentrant;
void dns_parse(void) __banked __reentrant;

#endif

#ifndef _RADIUS_H_
#define _RADIUS_H_

#include <stdint.h>
#include <stdbool.h>

#define RADIUS_PORT_DEFAULT 1812

#define RADIUS_CODE_ACCESS_REQUEST 1
#define RADIUS_CODE_ACCESS_ACCEPT 2
#define RADIUS_CODE_ACCESS_REJECT 3
#define RADIUS_CODE_ACCESS_CHALLENGE 11
#define RADIUS_CODE_TIMEOUT 0

#define RADIUS_ATTR_USER_NAME 1
#define RADIUS_ATTR_NAS_IP_ADDRESS 4
#define RADIUS_ATTR_NAS_PORT 5
#define RADIUS_ATTR_FRAMED_MTU 12
#define RADIUS_ATTR_STATE 24
#define RADIUS_ATTR_CALLING_STATION_ID 31
#define RADIUS_ATTR_NAS_PORT_TYPE 61
#define RADIUS_ATTR_EAP_MESSAGE 79
#define RADIUS_ATTR_MESSAGE_AUTHENTICATOR 80

#define RADIUS_NAS_PORT_TYPE_ETHERNET 15
#define RADIUS_FRAMED_MTU 1400

#define RADIUS_HEADER_LENGTH 20
#define RADIUS_AUTHENTICATOR_LENGTH 16
#define RADIUS_ATTR_VALUE_MAX 253
#define RADIUS_SECRET_MAX 32
#define RADIUS_STATE_MAX 48
#define RADIUS_REQUEST_MAX 1200
#define RADIUS_EAP_MAX 1024
#define RADIUS_RETRIES 3
#define RADIUS_TIMEOUT_S 3

struct radius_config {
    uint8_t server[4];		/* IPv4 address, all zero = not configured */
    uint16_t port;		/* UDP port, RADIUS_PORT_DEFAULT */
    char secret[RADIUS_SECRET_MAX + 1];	/* shared secret, NUL terminated */
};

extern __xdata struct radius_config radius_cfg;
extern __xdata uint8_t radius_eap[RADIUS_EAP_MAX];	/* EAP-Message of the last reply, reassembled */
extern __xdata uint16_t radius_eap_len;

void radius_init(void) __banked __reentrant;
void radius_server(__xdata uint8_t *ip, uint16_t port) __banked __reentrant;
bool radius_configured(void) __banked __reentrant;
bool radius_request(uint8_t port, __xdata const char *identity, __xdata const uint8_t *mac,
                    __xdata const uint8_t *eap, uint16_t eap_len,
                    __xdata const uint8_t *state, uint8_t state_len) __banked __reentrant;
void radius_callback(uint16_t lport) __banked __reentrant;
void radius_tick(void) __banked __reentrant;

#endif

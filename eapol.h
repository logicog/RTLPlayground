#ifndef _EAPOL_H_
#define _EAPOL_H_

#include <stdint.h>
#include <stdbool.h>
#include "rtl837x_dot1x.h"
#include "radius.h"

#define EAPOL_ETHERTYPE 0x888e
#define EAPOL_MULTICAST_ADDR "\x01\x80\xc2\x00\x00\x03"
#define EAPOL_VERSION 2
#define EAPOL_HEADER_LENGTH 4
#define EAPOL_TYPE_EAP 0
#define EAPOL_TYPE_START 1
#define EAPOL_TYPE_LOGOFF 2

#define EAP_CODE_REQUEST 1
#define EAP_CODE_RESPONSE 2
#define EAP_CODE_SUCCESS 3
#define EAP_CODE_FAILURE 4
#define EAP_HEADER_LENGTH 4
#define EAP_TYPE_IDENTITY 1

#define EAPOL_IDENTITY_MAX 32
#define EAPOL_TX_PERIOD_S 30
#define EAPOL_MAX_REQUESTS 3
#define EAPOL_SERVER_TIMEOUT_S 30
#define EAPOL_QUIET_S 60
#define EAPOL_REAUTH_S 3600

#define EAPOL_STATE_IDLE 0
#define EAPOL_STATE_CONNECTING 1
#define EAPOL_STATE_AUTHENTICATING 2
#define EAPOL_STATE_AUTHENTICATED 3
#define EAPOL_STATE_HELD 4

struct eapol_port {
    uint8_t state;		/* EAPOL_STATE_* */
    uint8_t eap_id;		/* identifier of the last request we originated */
    uint8_t resp_id;		/* identifier of the last response from the supplicant */
    uint8_t tries;		/* identity requests sent without an answer */
    uint16_t timer;		/* seconds left in the current state */
    uint8_t mac[6];		/* supplicant address */
    char identity[EAPOL_IDENTITY_MAX + 1];	/* NUL terminated EAP identity */
    uint8_t rstate[RADIUS_STATE_MAX];	/* RADIUS State to echo in the next request */
    uint8_t rstate_len;
    uint8_t mab;		/* 1 = fall back to MAC authentication when no supplicant answers */
    uint8_t via_mab;		/* 1 = the running or last authentication was by MAC */
    uint16_t ok;		/* successful authentications since boot */
    uint16_t fail;		/* rejected or timed out authentications since boot */
};

extern __xdata struct eapol_port eapol_ports[DOT1X_PORTS];

void eapol_init(void) __banked __reentrant;
bool eapol_in(void) __banked __reentrant;
void eapol_poll(void) __banked __reentrant;
void eapol_tick(void) __banked __reentrant;
void eapol_port_reset(uint8_t port) __banked __reentrant;
void eapol_port_mab(uint8_t port, bool on) __banked __reentrant;
void eapol_radius_result(uint8_t port, uint8_t code, __xdata const uint8_t *state, uint8_t state_len) __banked __reentrant;
__code const char *eapol_state_name(uint8_t port) __banked __reentrant;

#endif

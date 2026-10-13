#pragma codeseg BANK3
#pragma constseg BANK3

#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "uip.h"
#include "uip/uip.h"
#include "md5.h"
#include "radius.h"
#include "eapol.h"
#include "page_impl.h"
#include "machine.h"

extern __code const struct machine machine;

extern __xdata uint8_t sfr_data[4];
extern __xdata uip_ipaddr_t uip_hostaddr;

#define RADIUS_PHASE_IDLE 0
#define RADIUS_PHASE_SEND 1
#define RADIUS_PHASE_WAIT 2
#define RADIUS_NO_PORT 0xff

__xdata struct radius_config radius_cfg;
__xdata uint8_t radius_eap[RADIUS_EAP_MAX];
__xdata uint16_t radius_eap_len;

static __xdata uint8_t radius_req[RADIUS_REQUEST_MAX];
static __xdata uint16_t radius_req_len;
static __xdata struct uip_udp_conn *radius_conn;
static __xdata uip_ipaddr_t radius_ip;
static __xdata uint8_t radius_port_busy;
static __xdata uint8_t radius_id;
static __xdata uint8_t radius_phase;
static __xdata uint8_t radius_timer;
static __xdata uint8_t radius_tries;
static __xdata uint8_t radius_digest[MD5_DIGEST_LENGTH];
static __xdata uint8_t radius_saved[MD5_DIGEST_LENGTH];
static __xdata struct md5_ctx radius_md5;
static __xdata uint8_t radius_state[RADIUS_STATE_MAX];
static __xdata uint8_t radius_state_len;

void radius_init(void) __banked __reentrant
{
    radius_cfg.server[0] = 0;
    radius_cfg.server[1] = 0;
    radius_cfg.server[2] = 0;
    radius_cfg.server[3] = 0;
    radius_cfg.port = RADIUS_PORT_DEFAULT;
    radius_cfg.secret[0] = 0;
    radius_conn = 0;
    radius_port_busy = RADIUS_NO_PORT;
    radius_phase = RADIUS_PHASE_IDLE;
    radius_eap_len = 0;
}

void radius_server(__xdata uint8_t *ip, uint16_t port) __banked __reentrant
{
    memcpy(radius_cfg.server, ip, 4);
    radius_cfg.port = port;
    if (radius_conn)
        uip_udp_remove(radius_conn);
    uip_ipaddr(radius_ip, ip[0], ip[1], ip[2], ip[3]);
    radius_conn = uip_udp_new(&radius_ip, HTONS(port));
    if (!radius_conn)
        print_string("RADIUS: no free UDP connection\n");
}

bool radius_configured(void) __banked __reentrant
{
    return radius_conn && radius_cfg.secret[0];
}

static __xdata uint8_t *radius_attr(__xdata uint8_t *p, uint8_t type, __xdata const uint8_t *v, uint8_t len) __reentrant
{
    *p++ = type;
    *p++ = len + 2;
    memcpy(p, v, len);
    return p + len;
}

static __xdata uint8_t *radius_attr_u32(__xdata uint8_t *p, uint8_t type, uint32_t v) __reentrant
{
    *p++ = type;
    *p++ = 6;
    *p++ = v >> 24;
    *p++ = v >> 16;
    *p++ = v >> 8;
    *p++ = v;
    return p;
}

static uint8_t radius_secret_len(void) __reentrant
{
    return strlen_x(radius_cfg.secret);
}

bool radius_request(uint8_t port, __xdata const char *identity, __xdata const uint8_t *mac,
                    __xdata const uint8_t *eap, uint16_t eap_len,
                    __xdata const uint8_t *state, uint8_t state_len) __banked __reentrant
{
    __xdata uint8_t *p = radius_req;
    __xdata uint8_t *ma;
    uint8_t i;
    uint8_t chunk;

    if (radius_port_busy != RADIUS_NO_PORT || !radius_configured())
        return false;
    if (eap_len > RADIUS_EAP_MAX)
        return false;

    *p++ = RADIUS_CODE_ACCESS_REQUEST;
    *p++ = ++radius_id;
    p += 2;
    for (i = 0; i < RADIUS_AUTHENTICATOR_LENGTH; i += 4) {
        get_random_32();
        memcpy(p, sfr_data, 4);
        p += 4;
    }
    p = radius_attr(p, RADIUS_ATTR_USER_NAME, (__xdata const uint8_t *)identity, strlen_x(identity));
    p = radius_attr(p, RADIUS_ATTR_NAS_IP_ADDRESS, (__xdata const uint8_t *)uip_hostaddr, 4);
    p = radius_attr_u32(p, RADIUS_ATTR_NAS_PORT, machine.log_to_phys_port[port]);
    p = radius_attr_u32(p, RADIUS_ATTR_NAS_PORT_TYPE, RADIUS_NAS_PORT_TYPE_ETHERNET);
    p = radius_attr_u32(p, RADIUS_ATTR_FRAMED_MTU, RADIUS_FRAMED_MTU);
    *p++ = RADIUS_ATTR_CALLING_STATION_ID;
    *p++ = 2 + 17;
    for (i = 0; i < 6; i++) {
        if (i)
            *p++ = '-';
        *p++ = itohex(mac[i] >> 4);
        *p++ = itohex(mac[i]);
    }
    if (state_len)
        p = radius_attr(p, RADIUS_ATTR_STATE, state, state_len);
    while (eap_len) {
        chunk = eap_len > RADIUS_ATTR_VALUE_MAX ? RADIUS_ATTR_VALUE_MAX : eap_len;
        p = radius_attr(p, RADIUS_ATTR_EAP_MESSAGE, eap, chunk);
        eap += chunk;
        eap_len -= chunk;
    }
    ma = p;
    *p++ = RADIUS_ATTR_MESSAGE_AUTHENTICATOR;
    *p++ = 2 + MD5_DIGEST_LENGTH;
    memset(p, 0, MD5_DIGEST_LENGTH);
    p += MD5_DIGEST_LENGTH;

    radius_req_len = p - radius_req;
    radius_req[2] = radius_req_len >> 8;
    radius_req[3] = radius_req_len;
    hmac_md5((__xdata const uint8_t *)radius_cfg.secret, radius_secret_len(), radius_req, radius_req_len, ma + 2);

    radius_port_busy = port;
    radius_tries = 0;
    radius_phase = RADIUS_PHASE_SEND;
    return true;
}

static void radius_finish(uint8_t code) __reentrant
{
    uint8_t port = radius_port_busy;

    radius_port_busy = RADIUS_NO_PORT;
    radius_phase = RADIUS_PHASE_IDLE;
    eapol_radius_result(port, code, radius_state, radius_state_len);
}

static void radius_receive(void) __reentrant
{
    __xdata uint8_t *p = (__xdata uint8_t *)uip_appdata;
    __xdata uint8_t *end;
    __xdata uint8_t *a;
    uint16_t len = uip_datalen();
    uint8_t i;
    uint8_t alen;
    bool has_ma = false;

    if (radius_phase != RADIUS_PHASE_WAIT || len < RADIUS_HEADER_LENGTH || p[1] != radius_id)
        return;
    if ((((uint16_t)p[2] << 8) | p[3]) > len)
        return;
    len = ((uint16_t)p[2] << 8) | p[3];
    if (p[0] != RADIUS_CODE_ACCESS_ACCEPT && p[0] != RADIUS_CODE_ACCESS_REJECT && p[0] != RADIUS_CODE_ACCESS_CHALLENGE)
        return;

    md5_init(&radius_md5);
    md5_update(&radius_md5, p, 4);
    md5_update(&radius_md5, radius_req + 4, RADIUS_AUTHENTICATOR_LENGTH);
    md5_update(&radius_md5, p + RADIUS_HEADER_LENGTH, len - RADIUS_HEADER_LENGTH);
    md5_update(&radius_md5, (__xdata const uint8_t *)radius_cfg.secret, radius_secret_len());
    md5_final(&radius_md5, radius_digest);
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        if (radius_digest[i] != p[4 + i])
            return;
    }

    end = p + len;
    radius_eap_len = 0;
    radius_state_len = 0;
    for (a = p + RADIUS_HEADER_LENGTH; a + 2 <= end && a[1] >= 2 && a + a[1] <= end; a += a[1]) {
        alen = a[1] - 2;
        switch (a[0]) {
        case RADIUS_ATTR_MESSAGE_AUTHENTICATOR:
            if (alen == MD5_DIGEST_LENGTH) {
                memcpy(radius_saved, a + 2, MD5_DIGEST_LENGTH);
                memset(a + 2, 0, MD5_DIGEST_LENGTH);
                has_ma = true;
            }
            break;
        case RADIUS_ATTR_STATE:
            if (alen <= RADIUS_STATE_MAX) {
                memcpy(radius_state, a + 2, alen);
                radius_state_len = alen;
            }
            break;
        case RADIUS_ATTR_EAP_MESSAGE:
            if (radius_eap_len + alen <= RADIUS_EAP_MAX) {
                memcpy(radius_eap + radius_eap_len, a + 2, alen);
                radius_eap_len += alen;
            }
            break;
        }
    }
    if (!has_ma)
        return;
    memcpy(p + 4, radius_req + 4, RADIUS_AUTHENTICATOR_LENGTH);
    hmac_md5((__xdata const uint8_t *)radius_cfg.secret, radius_secret_len(), p, len, radius_digest);
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        if (radius_digest[i] != radius_saved[i])
            return;
    }
    radius_finish(p[0]);
}

void radius_callback(uint16_t lport) __banked __reentrant
{
    if (!radius_conn || lport != radius_conn->lport)
        return;
    if (uip_newdata()) {
        radius_receive();
        return;
    }
    if (uip_poll() && radius_phase == RADIUS_PHASE_SEND) {
        memcpy((__xdata uint8_t *)uip_appdata, radius_req, radius_req_len);
        uip_udp_send(radius_req_len);
        radius_phase = RADIUS_PHASE_WAIT;
        radius_timer = RADIUS_TIMEOUT_S;
    }
}

void radius_tick(void) __banked __reentrant
{
    if (radius_phase != RADIUS_PHASE_WAIT)
        return;
    if (--radius_timer)
        return;
    if (++radius_tries >= RADIUS_RETRIES) {
        radius_finish(RADIUS_CODE_TIMEOUT);
        return;
    }
    radius_phase = RADIUS_PHASE_SEND;
}

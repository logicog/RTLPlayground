#pragma codeseg BANK3
#pragma constseg BANK3

#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_trap.h"
#include "uip.h"
#include "uip/uip.h"
#include "machine.h"
#include "eapol.h"

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];

#define EAPOL_NO_PORT 0xff

struct eapol_frame {
    struct uip_eth_addr dst;
    struct uip_eth_addr src;
    struct rtl_tag rtl_tag;
    uint16_t ether_type;
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint8_t body[1];
};

#define EAPOL_O ((__xdata struct eapol_frame *)&uip_buf[RTL_FRAME_DESC_SIZE])
#define EAPOL_RX_TAG ((__xdata struct rtl_tag *)&uip_buf[2 * 6])
#define EAPOL_MIN_PAYLOAD 46

__xdata struct eapol_port eapol_ports[DOT1X_PORTS];

static __xdata uint8_t eapol_pending_port;
static __xdata uint8_t eapol_pending_type;
static __xdata uint8_t eapol_small[EAP_HEADER_LENGTH + 1];
static __xdata uint16_t eapol_links_last;
static __code const char * __code const eapol_state_names[5] = { "idle", "connecting", "authenticating", "authenticated", "held" };

static void eapol_send(uint8_t port, uint8_t type, __xdata const uint8_t *body, uint16_t len) __reentrant
{
    __xdata uint8_t *p;

    memcpyc(EAPOL_O->dst.addr, EAPOL_MULTICAST_ADDR, 6);
    memcpy(EAPOL_O->src.addr, uip_ethaddr.addr, 6);
    EAPOL_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
    EAPOL_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
    EAPOL_O->rtl_tag.reason = 0;
    EAPOL_O->rtl_tag.flags = HTONS(RTL_TAG_LEARN_DIS);
    EAPOL_O->rtl_tag.pmask = HTONS((uint16_t)1 << port);
    EAPOL_O->ether_type = HTONS(EAPOL_ETHERTYPE);
    EAPOL_O->version = EAPOL_VERSION;
    EAPOL_O->type = type;
    EAPOL_O->length = HTONS(len);
    p = EAPOL_O->body;
    memcpy(p, body, len);
    p += len;
    while (p < EAPOL_O->body - EAPOL_HEADER_LENGTH + EAPOL_MIN_PAYLOAD)
        *p++ = 0;
    uip_len = p - &uip_buf[RTL_FRAME_DESC_SIZE];
    tcpip_output();
}

static void eapol_queue(uint8_t port, uint8_t code, uint8_t id) __reentrant
{
    if (radius_eap_len == 0 || radius_eap[0] != code) {
        eapol_small[0] = code;
        eapol_small[1] = id;
        eapol_small[2] = 0;
        eapol_small[3] = EAP_HEADER_LENGTH;
        memcpy(radius_eap, eapol_small, EAP_HEADER_LENGTH);
        radius_eap_len = EAP_HEADER_LENGTH;
    }
    eapol_pending_port = port;
    eapol_pending_type = EAPOL_TYPE_EAP;
}

static void eapol_request_identity(uint8_t port) __reentrant
{
    __xdata struct eapol_port *e = &eapol_ports[port];

    e->eap_id++;
    eapol_small[0] = EAP_CODE_REQUEST;
    eapol_small[1] = e->eap_id;
    eapol_small[2] = 0;
    eapol_small[3] = EAP_HEADER_LENGTH + 1;
    eapol_small[4] = EAP_TYPE_IDENTITY;
    eapol_send(port, EAPOL_TYPE_EAP, eapol_small, EAP_HEADER_LENGTH + 1);
    e->tries++;
    e->timer = EAPOL_TX_PERIOD_S;
    if (e->state != EAPOL_STATE_AUTHENTICATED)
        e->state = EAPOL_STATE_CONNECTING;
}

static void eapol_log(uint8_t port, __code const char *what) __reentrant
{
    uint8_t phys = machine.log_to_phys_port[port];

    print_string("802.1X: port ");
    if (phys >= 10)
        write_char('1');
    write_char('0' + phys % 10);
    print_string(" ");
    print_string(what);
    print_string(" ");
    print_string_x(eapol_ports[port].identity);
    write_char('\n');
}

void eapol_init(void) __banked __reentrant
{
    memset(eapol_ports, 0, sizeof(eapol_ports));
    eapol_pending_port = EAPOL_NO_PORT;
    eapol_links_last = 0;
}

void eapol_port_reset(uint8_t port) __banked __reentrant
{
    memset(&eapol_ports[port], 0, sizeof(struct eapol_port));
    dot1x_authorize(port, false);
}

static uint16_t eapol_links(void) __reentrant
{
    reg_read_m(RTL837X_REG_LINKS_STS);
    return ((uint16_t)sfr_data[2] << 8) | sfr_data[1];
}

bool eapol_in(void) __banked __reentrant
{
    __xdata struct eapol_port *e;
    __xdata uint8_t *p;
    uint8_t port;
    uint8_t body = TRAP_RX_BODY();
    uint16_t len;

    if (uip_buf[body] != (EAPOL_ETHERTYPE >> 8) || uip_buf[body + 1] != (EAPOL_ETHERTYPE & 0xff))
        return false;
    port = ((uint8_t)HTONS(EAPOL_RX_TAG->pmask)) & 0x0f;
    if (!MACHINE_PORT_VALID(port))
        return true;
    if (!dot1x_enabled || dot1x_mode[port] != DOT1X_PORT_AUTO)
        return true;
    e = &eapol_ports[port];
    p = &uip_buf[body + 2];
    switch (p[1]) {
    case EAPOL_TYPE_START:
        memcpy(e->mac, &uip_buf[6], 6);
        e->tries = 0;
        eapol_request_identity(port);
        return true;
    case EAPOL_TYPE_LOGOFF:
        if (e->state == EAPOL_STATE_AUTHENTICATED)
            eapol_log(port, "logoff");
        eapol_port_reset(port);
        return true;
    case EAPOL_TYPE_EAP:
        break;
    default:
        return true;
    }
    len = ((uint16_t)p[2] << 8) | p[3];
    p += EAPOL_HEADER_LENGTH;
    if (len < EAP_HEADER_LENGTH || p[0] != EAP_CODE_RESPONSE)
        return true;
    len = ((uint16_t)p[2] << 8) | p[3];
    if (len < EAP_HEADER_LENGTH || &p[len] > &uip_buf[uip_len])
        return true;
    if (len > EAP_HEADER_LENGTH && p[4] == EAP_TYPE_IDENTITY) {
        uint8_t n = len - EAP_HEADER_LENGTH - 1;
        if (n > EAPOL_IDENTITY_MAX)
            n = EAPOL_IDENTITY_MAX;
        memcpy(e->identity, p + 5, n);
        e->identity[n] = 0;
        memcpy(e->mac, &uip_buf[6], 6);
        e->rstate_len = 0;
    }
    if (radius_request(port, e->identity, e->mac, p, len, e->rstate, e->rstate_len)) {
        e->resp_id = p[1];
        e->state = EAPOL_STATE_AUTHENTICATING;
        e->timer = EAPOL_SERVER_TIMEOUT_S;
    }
    return true;
}

void eapol_radius_result(uint8_t port, uint8_t code, __xdata const uint8_t *state, uint8_t state_len) __banked __reentrant
{
    __xdata struct eapol_port *e = &eapol_ports[port];

    if (!dot1x_enabled || dot1x_mode[port] != DOT1X_PORT_AUTO)
        return;
    switch (code) {
    case RADIUS_CODE_ACCESS_CHALLENGE:
        memcpy(e->rstate, state, state_len);
        e->rstate_len = state_len;
        if (radius_eap_len >= EAP_HEADER_LENGTH) {
            eapol_pending_port = port;
            eapol_pending_type = EAPOL_TYPE_EAP;
        }
        e->timer = EAPOL_SERVER_TIMEOUT_S;
        return;
    case RADIUS_CODE_ACCESS_ACCEPT:
        eapol_queue(port, EAP_CODE_SUCCESS, e->resp_id);
        dot1x_authorize(port, true);
        e->state = EAPOL_STATE_AUTHENTICATED;
        e->timer = EAPOL_REAUTH_S;
        e->tries = 0;
        e->rstate_len = 0;
        eapol_log(port, "authorized");
        return;
    default:
        eapol_queue(port, EAP_CODE_FAILURE, e->resp_id);
        dot1x_authorize(port, false);
        e->state = EAPOL_STATE_HELD;
        e->timer = EAPOL_QUIET_S;
        e->rstate_len = 0;
        eapol_log(port, code == RADIUS_CODE_TIMEOUT ? "no answer from the server for" : "rejected");
        return;
    }
}

void eapol_poll(void) __banked __reentrant
{
    uint8_t port = eapol_pending_port;

    if (port == EAPOL_NO_PORT)
        return;
    eapol_pending_port = EAPOL_NO_PORT;
    eapol_send(port, eapol_pending_type, radius_eap, radius_eap_len);
}

void eapol_tick(void) __banked __reentrant
{
    __xdata struct eapol_port *e;
    uint16_t links = eapol_links();
    uint8_t port;

    for (port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++) {
        e = &eapol_ports[port];
        if (!dot1x_enabled || dot1x_mode[port] != DOT1X_PORT_AUTO) {
            if (e->state != EAPOL_STATE_IDLE)
                eapol_port_reset(port);
            continue;
        }
        if (!(links & ((uint16_t)1 << port))) {
            if (e->state != EAPOL_STATE_IDLE)
                eapol_port_reset(port);
            continue;
        }
        if (e->state == EAPOL_STATE_IDLE) {
            e->tries = 0;
            eapol_request_identity(port);
            continue;
        }
        if (e->timer)
            e->timer--;
        if (e->timer)
            continue;
        switch (e->state) {
        case EAPOL_STATE_CONNECTING:
            if (e->tries >= EAPOL_MAX_REQUESTS)
                e->tries = 0;
            eapol_request_identity(port);
            break;
        case EAPOL_STATE_AUTHENTICATING:
            e->state = EAPOL_STATE_HELD;
            e->timer = EAPOL_QUIET_S;
            break;
        case EAPOL_STATE_HELD:
            e->state = EAPOL_STATE_IDLE;
            break;
        case EAPOL_STATE_AUTHENTICATED:
            if (e->tries >= EAPOL_MAX_REQUESTS) {
                eapol_log(port, "silent on reauthentication");
                eapol_port_reset(port);
                break;
            }
            eapol_request_identity(port);
            break;
        }
    }
    eapol_links_last = links;
}

__code const char *eapol_state_name(uint8_t port) __banked __reentrant
{
    return eapol_state_names[eapol_ports[port].state];
}

#pragma codeseg BANK3
#pragma constseg BANK3

#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_trap.h"
#include "rtl837x_dot1x.h"
#include "machine.h"
#include "cmd_parser.h"
#include "eapol.h"
#include "radius.h"
#include "timer.h"

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata uint8_t outbuf[];
extern __xdata uint16_t slen;

static __xdata struct timer dot1x_timer;

__xdata bool dot1x_enabled;
__xdata uint8_t dot1x_mode[DOT1X_PORTS];

static __code const char * __code const dot1x_mode_names[4] = { "off", "auto", "force-auth", "force-unauth" };
static __code const char * __code const dot1x_action_names[3] = { "drop", "trap", "guest" };

static void dot1x_reg_bit(uint16_t reg, uint8_t bit, bool on) __reentrant
{
    uint8_t i = 3 - (bit >> 3);
    uint8_t m = 1 << (bit & 7);

    reg_read_m(reg);
    if (on)
        sfr_data[i] |= m;
    else
        sfr_data[i] &= ~m;
    reg_write_m(reg);
}

static bool dot1x_reg_bit_get(uint16_t reg, uint8_t bit) __reentrant
{
    reg_read_m(reg);
    return (sfr_data[3 - (bit >> 3)] >> (bit & 7)) & 1;
}

static void dot1x_apply(uint8_t port) __reentrant
{
    uint8_t mode = dot1x_mode[port];
    bool on = dot1x_enabled && mode != DOT1X_PORT_OFF;

    dot1x_reg_bit(RTL837X_DOT1X_PORT_AUTH, port, mode == DOT1X_PORT_FORCE_AUTH
                  || (mode == DOT1X_PORT_AUTO && eapol_ports[port].state == EAPOL_STATE_AUTHENTICATED));
    dot1x_reg_bit(RTL837X_DOT1X_PORT_EN, port, on);
}

static uint8_t dot1x_action_get(uint8_t port) __reentrant
{
    uint8_t shift = port << 1;

    reg_read_m(RTL837X_DOT1X_UNAUTH_ACT);
    return (sfr_data[3 - (shift >> 3)] >> (shift & 7)) & 3;
}

static uint16_t dot1x_guest_get(void) __reentrant
{
    reg_read_m(RTL837X_DOT1X_CFG);
    return ((uint16_t)(sfr_data[2] & (RTL837X_DOT1X_CFG_GUEST_VID_MASK >> 8)) << 8) | sfr_data[3];
}

void dot1x_init(void) __banked
{
    uint8_t port;

    dot1x_enabled = false;
    for (port = 0; port < DOT1X_PORTS; port++)
        dot1x_mode[port] = DOT1X_PORT_OFF;
    timer_set(&dot1x_timer, SYS_TICK_HZ);
    radius_init();
    eapol_init();
    dot1x_enable(false);
}

void dot1x_authorize(uint8_t port, bool on) __banked
{
    if (dot1x_mode[port] == DOT1X_PORT_AUTO)
        dot1x_reg_bit(RTL837X_DOT1X_PORT_AUTH, port, on);
}

void dot1x_tick(void) __banked
{
    eapol_poll();
    if (!timer_expired(&dot1x_timer))
        return;
    timer_reset(&dot1x_timer);
    radius_tick();
    eapol_tick();
}

void dot1x_enable(bool on) __banked
{
    uint8_t port;

    dot1x_enabled = on;
    trap_rma_set(RTL837X_RMA3_CONF, on ? RTL837X_RMA_ACT_TRAP : RTL837X_RMA_ACT_FORWARD);
    for (port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++)
        dot1x_apply(port);
}

void dot1x_port_mode(uint8_t port, uint8_t mode) __banked
{
    dot1x_mode[port] = mode;
    eapol_port_reset(port);
    dot1x_apply(port);
}

void dot1x_port_action(uint8_t port, uint8_t action) __banked
{
    uint8_t shift = port << 1;
    uint8_t i = 3 - (shift >> 3);
    uint8_t bit = shift & 7;

    reg_read_m(RTL837X_DOT1X_UNAUTH_ACT);
    sfr_data[i] = (sfr_data[i] & ~(3 << bit)) | (action << bit);
    reg_write_m(RTL837X_DOT1X_UNAUTH_ACT);
}

void dot1x_guest_vlan(uint16_t vid) __banked
{
    reg_read_m(RTL837X_DOT1X_CFG);
    sfr_data[2] = (sfr_data[2] & ~(RTL837X_DOT1X_CFG_GUEST_VID_MASK >> 8)) | (vid >> 8);
    sfr_data[3] = vid & 0xff;
    reg_write_m(RTL837X_DOT1X_CFG);
}

static void dot1x_out(uint8_t c) __reentrant
{
    outbuf[slen++] = c;
}

static void dot1x_out_dec(uint16_t v) __reentrant
{
    uint8_t digits[5];
    uint8_t n = 0;

    do {
        digits[n++] = '0' + v % 10;
        v /= 10;
    } while (v);
    while (n)
        dot1x_out(digits[--n]);
}

static void dot1x_out_code(__code const char *s) __reentrant
{
    slen += strtox(outbuf + slen, s);
}

void dot1x_show(void) __banked __reentrant
{
    uint8_t port;

    print_string(dot1x_enabled ? "802.1X on, guest VLAN " : "802.1X off, guest VLAN ");
    itoa_short(dot1x_guest_get());
    print_string(", RADIUS ");
    print_ip(radius_cfg.server);
    write_char(':');
    itoa_short(radius_cfg.port);
    print_string(radius_cfg.secret[0] ? " with secret" : " without secret");
    print_string("\nPort  Mode  Unauthorized  Status  Supplicant\n");
    for (port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++) {
        itoa(machine.log_to_phys_port[port]);
        print_string("  ");
        print_string(dot1x_mode_names[dot1x_mode[port]]);
        print_string("  ");
        print_string(dot1x_action_names[dot1x_action_get(port)]);
        print_string("  ");
        if (!dot1x_enabled || dot1x_mode[port] == DOT1X_PORT_OFF)
            print_string("open");
        else
            print_string(dot1x_reg_bit_get(RTL837X_DOT1X_PORT_AUTH, port) ? "authorized" : "unauthorized");
        if (dot1x_mode[port] == DOT1X_PORT_AUTO) {
            print_string("  ");
            print_string(eapol_state_name(port));
            print_string(" ");
            print_string_x(eapol_ports[port].identity);
            print_string(eapol_ports[port].mab ? "  mab" : "");
            print_string("  ok ");
            itoa_short(eapol_ports[port].ok);
            print_string(" fail ");
            itoa_short(eapol_ports[port].fail);
        }
        write_char('\n');
    }
}

void dot1x_to_html(void) __banked __reentrant
{
    uint8_t port;
    uint8_t i;

    dot1x_out_code("{\"on\":");
    dot1x_out('0' + dot1x_enabled);
    dot1x_out_code(",\"guest\":");
    dot1x_out_dec(dot1x_guest_get());
    dot1x_out_code(",\"ports\":[");
    for (port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++) {
        if (port > MACHINE_MIN_PORT)
            dot1x_out(',');
        dot1x_out_code("{\"portNum\":");
        dot1x_out_dec(machine.log_to_phys_port[port]);
        dot1x_out_code(",\"mode\":\"");
        dot1x_out_code(dot1x_mode_names[dot1x_mode[port]]);
        dot1x_out_code("\",\"action\":\"");
        dot1x_out_code(dot1x_action_names[dot1x_action_get(port)]);
        dot1x_out_code("\",\"authorized\":");
        dot1x_out('0' + (!dot1x_enabled || dot1x_mode[port] == DOT1X_PORT_OFF
                         || dot1x_reg_bit_get(RTL837X_DOT1X_PORT_AUTH, port)));
        if (dot1x_mode[port] == DOT1X_PORT_AUTO) {
            dot1x_out_code(",\"state\":\"");
            dot1x_out_code(eapol_state_name(port));
            dot1x_out_code("\",\"identity\":\"");
            for (i = 0; eapol_ports[port].identity[i]; i++)
                dot1x_out(eapol_ports[port].identity[i] < ' ' || eapol_ports[port].identity[i] > '~'
                          || eapol_ports[port].identity[i] == '"' || eapol_ports[port].identity[i] == '\\'
                          ? '.' : eapol_ports[port].identity[i]);
            dot1x_out('"');
            dot1x_out_code(",\"mab\":");
            dot1x_out('0' + eapol_ports[port].mab);
            dot1x_out_code(",\"viaMab\":");
            dot1x_out('0' + eapol_ports[port].via_mab);
            dot1x_out_code(",\"ok\":");
            dot1x_out_dec(eapol_ports[port].ok);
            dot1x_out_code(",\"fail\":");
            dot1x_out_dec(eapol_ports[port].fail);
        }
        dot1x_out('}');
    }
    dot1x_out_code("],\"server\":\"");
    for (i = 0; i < 4; i++) {
        if (i)
            dot1x_out('.');
        dot1x_out_dec(radius_cfg.server[i]);
    }
    dot1x_out_code("\",\"serverPort\":");
    dot1x_out_dec(radius_cfg.port);
    dot1x_out_code(",\"secretSet\":");
    dot1x_out('0' + (radius_cfg.secret[0] != 0));
    dot1x_out('}');
}

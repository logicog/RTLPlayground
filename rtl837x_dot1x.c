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

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata uint8_t outbuf[];
extern __xdata uint16_t slen;
extern volatile __xdata uint32_t ticks;

static __xdata uint8_t dot1x_tick_last;

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

    dot1x_reg_bit(RTL837X_DOT1X_PORT_AUTH, port, mode == DOT1X_PORT_FORCE_AUTH);
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
    dot1x_tick_last = (uint8_t)ticks;
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
    if (!TICKS_DUE(dot1x_tick_last, SYS_TICK_HZ))
        return;
    dot1x_tick_last += SYS_TICK_HZ;
    radius_tick();
    eapol_tick();
}

void dot1x_enable(bool on) __banked
{
    uint8_t port;

    dot1x_enabled = on;
    trap_rma_set(RTL837X_RMA3_CONF, on ? RTL837X_RMA_ACT_TRAP : RTL837X_RMA_ACT_FORWARD);
    for (port = machine.min_port; port <= machine.max_port; port++)
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

static void dot1x_put(uint8_t c, bool html) __reentrant
{
    if (html)
        outbuf[slen++] = c;
    else
        write_char(c);
}

static void dot1x_put_dec(uint16_t v, bool html) __reentrant
{
    uint8_t digits[5];
    uint8_t n = 0;

    do {
        digits[n++] = '0' + v % 10;
        v /= 10;
    } while (v);
    while (n)
        dot1x_put(digits[--n], html);
}

static void dot1x_put_code(__code const char *s, bool html) __reentrant
{
    if (html)
        slen += strtox(outbuf + slen, s);
    else
        print_string(s);
}

void dot1x_show(void) __banked __reentrant
{
    uint8_t port;

    print_string(dot1x_enabled ? "802.1X on, guest VLAN " : "802.1X off, guest VLAN ");
    dot1x_put_dec(dot1x_guest_get(), false);
    print_string(", RADIUS ");
    print_ip(radius_cfg.server);
    write_char(':');
    dot1x_put_dec(radius_cfg.port, false);
    print_string(radius_cfg.secret[0] ? " with secret" : " without secret");
    print_string("\nPort  Mode  Unauthorized  Status  Supplicant\n");
    for (port = machine.min_port; port <= machine.max_port; port++) {
        dot1x_put_dec(machine.log_to_phys_port[port], false);
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
        }
        write_char('\n');
    }
}

void dot1x_to_html(void) __banked __reentrant
{
    uint8_t port;
    uint8_t i;

    dot1x_put_code("{\"on\":", true);
    dot1x_put('0' + dot1x_enabled, true);
    dot1x_put_code(",\"guest\":", true);
    dot1x_put_dec(dot1x_guest_get(), true);
    dot1x_put_code(",\"ports\":[", true);
    for (port = machine.min_port; port <= machine.max_port; port++) {
        if (port > machine.min_port)
            dot1x_put(',', true);
        dot1x_put_code("{\"portNum\":", true);
        dot1x_put_dec(machine.log_to_phys_port[port], true);
        dot1x_put_code(",\"mode\":\"", true);
        dot1x_put_code(dot1x_mode_names[dot1x_mode[port]], true);
        dot1x_put_code("\",\"action\":\"", true);
        dot1x_put_code(dot1x_action_names[dot1x_action_get(port)], true);
        dot1x_put_code("\",\"authorized\":", true);
        dot1x_put('0' + (!dot1x_enabled || dot1x_mode[port] == DOT1X_PORT_OFF
                         || dot1x_reg_bit_get(RTL837X_DOT1X_PORT_AUTH, port)), true);
        if (dot1x_mode[port] == DOT1X_PORT_AUTO) {
            dot1x_put_code(",\"state\":\"", true);
            dot1x_put_code(eapol_state_name(port), true);
            dot1x_put_code("\",\"identity\":\"", true);
            for (i = 0; eapol_ports[port].identity[i]; i++)
                dot1x_put(eapol_ports[port].identity[i] < ' ' || eapol_ports[port].identity[i] > '~'
                          || eapol_ports[port].identity[i] == '"' || eapol_ports[port].identity[i] == '\\'
                          ? '.' : eapol_ports[port].identity[i], true);
            dot1x_put('"', true);
        }
        dot1x_put('}', true);
    }
    dot1x_put_code("],\"server\":\"", true);
    for (i = 0; i < 4; i++) {
        if (i)
            dot1x_put('.', true);
        dot1x_put_dec(radius_cfg.server[i], true);
    }
    dot1x_put_code("\",\"serverPort\":", true);
    dot1x_put_dec(radius_cfg.port, true);
    dot1x_put_code(",\"secretSet\":", true);
    dot1x_put('0' + (radius_cfg.secret[0] != 0), true);
    dot1x_put('}', true);
}

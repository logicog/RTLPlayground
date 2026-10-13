#ifndef _RTL837X_DOT1X_H_
#define _RTL837X_DOT1X_H_

#include <stdint.h>
#include <stdbool.h>

#define DOT1X_PORTS 10

#define DOT1X_PORT_OFF 0
#define DOT1X_PORT_AUTO 1
#define DOT1X_PORT_FORCE_AUTH 2
#define DOT1X_PORT_FORCE_UNAUTH 3

#define DOT1X_UNAUTH_DROP 0
#define DOT1X_UNAUTH_TRAP 1
#define DOT1X_UNAUTH_GUEST 2

extern __xdata bool dot1x_enabled;
extern __xdata uint8_t dot1x_mode[DOT1X_PORTS];	/* DOT1X_PORT_* per logical port */

void dot1x_init(void) __banked;
void dot1x_enable(bool on) __banked;
void dot1x_port_mode(uint8_t port, uint8_t mode) __banked;
void dot1x_port_action(uint8_t port, uint8_t action) __banked;
void dot1x_guest_vlan(uint16_t vid) __banked;
void dot1x_show(void) __banked;
void dot1x_to_html(void) __banked;
void dot1x_authorize(uint8_t port, bool on) __banked;
void dot1x_tick(void) __banked;

#endif

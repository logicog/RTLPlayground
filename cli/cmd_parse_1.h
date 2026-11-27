#ifndef _CMD_PARSE_1_H
#define _CMD_PARSE_1_H

// #define DEBUG
// #define REGDBG 1

#include "dns.h"
#include "machine.h"
#include "ntp.h"
#include "phy.h"
#include "rtl837x_bandwidth.h"
#include "rtl837x_cabletest.h"
#include "rtl837x_common.h"
#include "rtl837x_flash.h"
#include "rtl837x_igmp.h"
#include "rtl837x_lacp.h"
#include "rtl837x_leds.h"
#include "rtl837x_lldp.h"
#include "rtl837x_phy.h"
#include "rtl837x_port.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"
#include "rtl837x_storm.h"
#include "rtl837x_stp.h"
#include "rtl837x_trap.h"
#include "sessions.h"
#include "sfp.h"
#include <stdint.h>
#include "uip/uip.h"
#include "version.h"


void cmd_parse_bw(void);
void cmd_parse_dns(void) __banked __reentrant;
void cmd_parse_eee(void);
void cmd_parse_factory_reset(void);
void cmd_parse_flash(void);
void cmd_parse_gpio(void);
void cmd_parse_gw(void);
void cmd_parse_history(void);
void cmd_history_save(void);
void cmd_parse_hostname(void);
void cmd_parse_igmp(void);
void cmd_parse_ingress(void);
void cmd_parse_ip(void);
void cmd_parse_isolate(void);
void cmd_parse_l2(void);
void cmd_parse_lacp(void);
void cmd_parse_lag(void);
void cmd_parse_laghash(void);
void cmd_parse_leds(void);
void cmd_parse_lldp(void);
void cmd_parse_mac(void);
void cmd_parse_mirror(void);
void cmd_parse_mtu(void);
void cmd_parse_netmask(void);
void cmd_parse_ntp(void) __banked __reentrant;
void cmd_parse_passwd(void);
void cmd_parse_phyget(void);
void cmd_parse_physet(void);
void cmd_parse_port(void);
void cmd_parse_pvid(void);
void cmd_parse_regget(void);
void cmd_parse_regset(void);
void cmd_parse_reset(void);
void cmd_parse_rnd(void);
void cmd_parse_sdsget(void);
void cmd_parse_sds(void);
void cmd_parse_sdsset(void);
void cmd_parse_session(void);
void cmd_parse_sfp(void);
void cmd_parse_stat(void);
void cmd_parse_storm(void);
void cmd_parse_stp(void) __banked __reentrant;
void cmd_parse_syslog(void);
void cmd_parse_temp(void);
void cmd_parse_version(void);
void cmd_parse_vlan(void);

void clear_command_history(void) __banked;
void print_sw_version(void) __banked;

#endif
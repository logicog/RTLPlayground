#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_acl.h"
#include "rtl837x_dhcp_snoop.h"
#include "uip.h"
#include "cmd_parser.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

#define DS_IP_OFFSET	(12 + RTL_TAG_SIZE + VLAN_TAG_SIZE + 2)
#define DS_BOOTP_MIN	240
#define DS_IP_PROTO_UDP	17
#define DS_FWD_DROP	(ACL_FWD_REDIRECT * 0x20000UL)
#define DS_FWD_COPY_CPU	((ACL_FWD_COPY * 0x20000UL) | (0x200000UL << CPU_PORT))

extern __xdata uint8_t uip_buf[];
extern __code const struct machine machine;

__xdata uint8_t  dhcp_snoop_on;
__xdata uint16_t dhcp_snoop_trust;
__xdata struct dhcp_binding dhcp_bind[DHCP_BIND_MAX];

static __xdata uint8_t * __xdata ds_p;
static __xdata struct dhcp_binding * __xdata ds_b;
static __xdata uint16_t ds_i;
static __xdata uint16_t ds_len;
static __xdata uint16_t ds_vid;
static __xdata uint32_t ds_lease;
static __xdata uint8_t  ds_msg;
static __xdata uint8_t  ds_port;
static __xdata uint16_t ds_in;
static __xdata uint16_t ds_sport;
static __xdata uint32_t ds_fwd;
static __xdata struct dhcp_binding * __xdata ds_free;
static __xdata uint16_t ds_trusted;
static __xdata uint16_t ds_untrusted;
static __xdata uint8_t  ds_j;
static __xdata uint8_t  ds_c;
static __xdata uint8_t  ds_l;
static __xdata uint8_t  ds_alloc;
static __xdata uint8_t  ds_idx;
static __xdata uint16_t ds_e;

static void ds_find(void)
{
	ds_free = 0;
	for (ds_j = 0; ds_j < DHCP_BIND_MAX; ds_j++) {
		ds_b = &dhcp_bind[ds_j];
		if (ds_b->state == DHCP_BIND_FREE) {
			if (!ds_free)
				ds_free = ds_b;
		} else if (ds_b->vid == ds_vid && !memcmp(ds_b->mac, ds_p + 28, 6)) {
			return;
		}
	}
	ds_b = ds_alloc ? ds_free : 0;
	if (!ds_b)
		return;
	memcpy(ds_b->mac, ds_p + 28, 6);
	memset(ds_b->ip, 0, 4);
	ds_b->vid = ds_vid;
	ds_b->port = DHCP_BIND_PORT_NONE;
	ds_b->state = DHCP_BIND_PENDING;
	ds_b->secs = DHCP_PENDING_SECS;
}

static void ds_rule(void)
{
	acl_match_begin();
	acl_info_v = ACL_L3_IPV4 | ACL_L4_UDP;
	acl_info_m = ACL_INFO_L3 | ACL_INFO_L4;
	acl_vk = ds_sport;
	acl_mk = 0xffff;
	acl_match_key(ACL_FT_L4SPORT);
	acl_in_pmask = ds_in;
	acl_act.fwd_qos = ds_fwd;
	acl_ctrl = ACL_ACT_FWD;
	acl_rule_set(ds_idx);
}

void dhcp_snoop_apply(void) __banked
{
	ds_trusted = dhcp_snoop_trust & PMASK_9;
	ds_untrusted = PMASK_9 & ~ds_trusted;

	if (!dhcp_snoop_on || !ds_trusted || !ds_untrusted) {
		acl_rule_clear(DHCP_SNOOP_RULE_DROP);
		acl_rule_clear(DHCP_SNOOP_RULE_SERVER);
		acl_rule_clear(DHCP_SNOOP_RULE_CLIENT);
		for (ds_j = 0; ds_j < DHCP_BIND_MAX; ds_j++)
			dhcp_bind[ds_j].state = DHCP_BIND_FREE;
		return;
	}
	ds_sport = DHCP_SERVER_PORT;
	ds_in = ds_untrusted;
	ds_fwd = DS_FWD_DROP;
	ds_idx = DHCP_SNOOP_RULE_DROP;
	ds_rule();

	ds_in = ds_trusted;
	ds_fwd = DS_FWD_COPY_CPU;
	ds_idx = DHCP_SNOOP_RULE_SERVER;
	ds_rule();

	ds_sport = DHCP_CLIENT_PORT;
	ds_in = ds_untrusted;
	ds_idx = DHCP_SNOOP_RULE_CLIENT;
	ds_rule();
}

static void ds_secs(void)
{
	ds_b->secs = ds_lease;
}

static uint8_t ds_parse(void)
{
	ds_p = &uip_buf[DS_IP_OFFSET];
	if ((ds_p[0] & 0xf0) != 0x40 || ds_p[9] != DS_IP_PROTO_UDP)
		return 0;
	ds_p += (ds_p[0] & 0x0f) << 2;
	ds_i = ((uint16_t)ds_p[0] << 8) | ds_p[1];
	if (ds_i != DHCP_SERVER_PORT && ds_i != DHCP_CLIENT_PORT)
		return 0;
	ds_p += 8;
	ds_i = ds_p - uip_buf;
	ds_e = ds_i;
	ds_e += DS_BOOTP_MIN;
	if (uip_len < ds_e)
		return 0;
	ds_len = uip_len - ds_i;
	if (ds_p[1] != 1 || ds_p[2] != 6 || ds_p[236] != 0x63 || ds_p[237] != 0x82
	    || ds_p[238] != 0x53 || ds_p[239] != 0x63)
		return 0;

	ds_msg = 0;
	ds_lease = DHCP_LEASE_INFINITE;
	for (ds_i = DS_BOOTP_MIN;;) {
		ds_e = ds_i;
		ds_e++;
		if (ds_e >= ds_len)
			break;
		ds_c = ds_p[ds_i];
		if (ds_c == 255)
			break;
		if (!ds_c) {
			ds_i++;
			continue;
		}
		ds_l = ds_p[ds_e];
		ds_e++;
		ds_e += ds_l;
		if (ds_e > ds_len)
			break;
		if (ds_c == 53 && ds_l)
			ds_msg = ds_p[ds_i + 2];
		else if (ds_c == 51 && ds_l == 4)
			for (ds_j = 2, ds_lease = 0; ds_j < 6; ds_j++) {
				ds_lease <<= 8;
				ds_lease |= ds_p[ds_i + ds_j];
			}
		ds_i = ds_e;
	}

	ds_port = uip_buf[12 + 7] & 0x0f;
	ds_vid = (((uint16_t)uip_buf[12 + RTL_TAG_SIZE + 2] << 8) | uip_buf[12 + RTL_TAG_SIZE + 3]) & 0x0fff;
	return 1;
}

void dhcp_snoop_in(void) __banked
{
	if (!ds_parse())
		return;
	if (ds_p[0] == 1) {
		if (dhcp_snoop_trust & ((uint16_t)1 << ds_port))
			return;
		if (ds_msg == 1 || ds_msg == 3) {
			ds_alloc = 1;
			ds_find();
			if (ds_b) {
				ds_b->port = ds_port;
				if (ds_b->state == DHCP_BIND_PENDING)
					ds_b->secs = DHCP_PENDING_SECS;
			}
		} else if (ds_msg == 4 || ds_msg == 7) {
			ds_alloc = 0;
			ds_find();
			if (ds_b)
				ds_b->state = DHCP_BIND_FREE;
		}
	} else if (ds_p[0] == 2 && (dhcp_snoop_trust & ((uint16_t)1 << ds_port))) {
		ds_alloc = 0;
		ds_find();
		if (!ds_b)
			return;
		if (ds_msg == 5) {
			memcpy(ds_b->ip, ds_p + 16, 4);
			ds_secs();
			ds_b->state = DHCP_BIND_BOUND;
		} else if (ds_msg == 6) {
			ds_b->state = DHCP_BIND_FREE;
		}
	}
}

void dhcp_snoop_tick(void) __banked
{
	for (ds_j = 0; ds_j < DHCP_BIND_MAX; ds_j++) {
		ds_b = &dhcp_bind[ds_j];
		if (ds_b->state == DHCP_BIND_FREE || ds_b->secs == DHCP_LEASE_INFINITE)
			continue;
		if (!ds_b->secs || !--ds_b->secs)
			ds_b->state = DHCP_BIND_FREE;
	}
}

void dhcp_snoop_show(void) __banked
{
	for (ds_j = 0; ds_j < DHCP_BIND_MAX; ds_j++) {
		ds_b = &dhcp_bind[ds_j];
		if (ds_b->state == DHCP_BIND_FREE)
			continue;
		print_mac(ds_b->mac);
		write_char(' ');
		if (ds_b->state == DHCP_BIND_BOUND)
			print_ip(ds_b->ip);
		else
			print_string("pending");
		print_string(" vlan ");
		itoa_short(ds_b->vid);
		print_string(" port ");
		if (ds_b->port == DHCP_BIND_PORT_NONE)
			write_char('-');
		else
			write_char('0' + machine.log_to_phys_port[ds_b->port]);
		print_string(" lease ");
		ds_lease = ds_b->secs;
		if (ds_lease == DHCP_LEASE_INFINITE) {
			print_string("infinite\n");
			continue;
		}
		if (ds_lease >= 0x800000) {
			print_string("> 97 d\n");
			continue;
		}
		if (ds_lease < 0x10000) {
			ds_i = ds_lease;
			itoa_short(ds_i);
			print_string(" s\n");
		} else if (ds_lease < 0x40000) {
			ds_lease >>= 2;
			ds_i = ds_lease;
			ds_i /= 15;
			itoa_short(ds_i);
			print_string(" min\n");
		} else if (ds_lease < 0x100000) {
			ds_lease >>= 4;
			ds_i = ds_lease;
			ds_i /= 225;
			itoa_short(ds_i);
			print_string(" h\n");
		} else {
			ds_lease >>= 7;
			ds_i = ds_lease;
			ds_i /= 675;
			itoa_short(ds_i);
			print_string(" d\n");
		}
	}
}

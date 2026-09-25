#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "machine.h"
#include "cmd_parser.h"
#include "rtl837x_acl.h"

#pragma codeseg BANK3
#pragma constseg BANK3

#define ACL_BAD		0xff

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[15];

static __xdata uint8_t  acl_w;
static __xdata uint8_t  acl_p;
static __xdata uint8_t  acl_c;
static __xdata uint8_t  acl_ft;
static __xdata uint8_t  acl_kind;
static __xdata uint8_t  acl_r;
static __xdata uint8_t  acl_rn;
static __xdata uint8_t  acl_cont;
static __xdata uint8_t  acl_vlan;
static __xdata uint8_t  acl_tag;
static __xdata uint16_t acl_pm;
static __xdata uint32_t acl_n;
static __xdata uint32_t acl_max;
static __xdata uint32_t acl_lo;
static __xdata uint32_t acl_hi;
static __xdata uint32_t acl_m;
static __xdata uint32_t acl_t32;
static __xdata uint8_t  acl_byp;
static __xdata uint8_t  acl_svlan;
static __xdata uint8_t  acl_intr;
static __xdata uint8_t  acl_pol[3];
static __xdata uint8_t  acl_mac[6];
static __xdata uint8_t  acl_mmask[6];
static __xdata uint8_t * __xdata acl_out;

static uint8_t acl_eq(__code const char *s)
{
	uint8_t i = cmd_words_b[acl_w];

	while (*s)
		if (cmd_buffer[i++] != *s++)
			return 0;
	i = cmd_buffer[i];
	return i == ' ' || !i;
}

static uint8_t acl_next(void)
{
	if (++acl_w >= cmd_words_len)
		return 0;
	acl_p = cmd_words_b[acl_w];
	return 1;
}

static uint8_t acl_end(void)
{
	uint8_t c = cmd_buffer[acl_p];

	return c == ' ' || !c;
}

static uint8_t acl_sep(uint8_t c)
{
	if (cmd_buffer[acl_p] != c)
		return 0;
	acl_p++;
	return 1;
}

static uint8_t acl_dec(void)
{
	uint8_t d = 0;

	acl_n = 0;
	for (;;) {
		uint8_t c = cmd_buffer[acl_p] - '0';
		if (c > 9)
			return d;
		if (++d > 9)
			return 0;
		acl_t32 = acl_n;
		acl_n <<= 2;
		acl_n += acl_t32;
		acl_n <<= 1;
		acl_n += c;
		acl_p++;
	}
}

static uint8_t acl_hex(void)
{
	uint8_t d = 0;

	acl_n = 0;
	for (;;) {
		uint8_t c = cmd_buffer[acl_p];
		if (c >= '0' && c <= '9') {
			c -= '0';
		} else {
			c |= 0x20;
			if (c < 'a' || c > 'f')
				return d;
			c -= 'a' - 10;
		}
		if (++d > 8)
			return 0;
		acl_n <<= 4;
		acl_n |= c;
		acl_p++;
	}
}

static uint8_t acl_val(void)
{
	if (!acl_next() || !acl_dec() || !acl_end())
		return 0;
	return acl_n <= acl_max;
}

static uint8_t acl_range(void)
{
	if (!acl_next() || !acl_dec())
		return 0;
	acl_lo = acl_hi = acl_n;
	if (acl_sep('-')) {
		if (!acl_dec())
			return 0;
		acl_hi = acl_n;
	}
	if (!acl_end() || acl_lo > acl_hi)
		return 0;
	return acl_hi <= acl_max;
}

static uint8_t acl_hexm(void)
{
	if (!acl_next() || !acl_hex() || acl_n > acl_max)
		return 0;
	acl_lo = acl_n;
	acl_m = acl_max;
	if (acl_sep('/')) {
		if (!acl_hex() || acl_n > acl_max)
			return 0;
		acl_m = acl_n;
	}
	return acl_end();
}

static uint8_t acl_ipv4(void)
{
	acl_m = 0;
	for (acl_c = 0; acl_c < 4; acl_c++) {
		if (acl_c && !acl_sep('.'))
			return 0;
		if (!acl_dec() || acl_n > 255)
			return 0;
		acl_m <<= 8;
		acl_m |= acl_n;
	}
	acl_n = acl_m;
	return 1;
}

static uint8_t acl_ip(void)
{
	if (!acl_next() || !acl_ipv4())
		return 0;
	acl_lo = acl_hi = acl_n;
	acl_m = 0xffffffff;
	if (acl_sep('-')) {
		if (!acl_ipv4() || acl_n < acl_lo)
			return 0;
		acl_hi = acl_n;
		acl_m = 0;
	} else if (acl_sep('/')) {
		if (!acl_dec() || acl_n > 32)
			return 0;
		acl_r = acl_n;
		acl_m = 0;
		for (acl_c = 0; acl_c < acl_r; acl_c++) {
			acl_m >>= 1;
			acl_m |= 0x80000000;
		}
		acl_lo &= acl_m;
		acl_hi = ~acl_m;
		acl_hi |= acl_lo;
	}
	return acl_end();
}

static uint8_t acl_mac6(void)
{
	for (acl_c = 0; acl_c < 6; acl_c++) {
		if (acl_c && !acl_sep(':') && !acl_sep('-'))
			return 0;
		acl_r = acl_p;
		if (!acl_hex() || acl_p - acl_r != 2)
			return 0;
		acl_out[acl_c] = acl_n;
	}
	return 1;
}

static uint8_t acl_macm(void)
{
	acl_out = acl_mac;
	if (!acl_next() || !acl_mac6())
		return 0;
	for (acl_c = 0; acl_c < 6; acl_c++)
		acl_mmask[acl_c] = 0xff;
	acl_out = acl_mmask;
	if (acl_sep('/') && !acl_mac6())
		return 0;
	return acl_end();
}

static uint8_t acl_port(void)
{
	uint8_t port = cmd_buffer[acl_p] - '1';

	if (port > 8)
		return ACL_BAD;
	port = machine.phys_to_log_port[port];
	if (port < machine.min_port || port > machine.max_port)
		return ACL_BAD;
	acl_p++;
	return port;
}

static uint8_t acl_ports(void)
{
	acl_pm = 0;
	do {
		acl_c = acl_port();
		if (acl_c == ACL_BAD)
			return 0;
		acl_pm |= (uint16_t)1 << acl_c;
	} while (acl_sep(','));
	return acl_end();
}

static void acl_key_set(void)
{
	acl_vk = acl_lo;
	acl_mk = acl_m;
	acl_match_key(acl_ft);
}

static uint8_t acl_rq_set(uint8_t key)
{
	acl_rq.kind = acl_kind;
	acl_rq.lo = acl_lo;
	acl_rq.hi = acl_hi;
	return acl_match_range(key);
}

static uint8_t acl_ip_key(void)
{
	acl_kind = ACL_RNG_IP;
	if (!acl_m)
		return acl_rq_set(ACL_BAD);
	acl_key_set();
	acl_vk = acl_lo >> 16;
	acl_mk = acl_m >> 16;
	acl_match_key(acl_ft + 1);
	return acl_rq_set(acl_key(acl_ft));
}

static uint8_t acl_exact(void)
{
	if (acl_lo != acl_hi)
		return acl_rq_set(ACL_BAD);
	acl_key_set();
	return acl_rq_set(acl_key(acl_ft));
}

static void acl_info(uint16_t v)
{
	acl_info_v = (acl_info_v & ~acl_pm) | v;
	acl_info_m |= acl_pm;
}

static uint8_t acl_flag(void)
{
	acl_pm = ACL_INFO_L3;
	if (acl_eq("nonip"))
		acl_info(ACL_L3_OTHER);
	else if (acl_eq("arp"))
		acl_info(ACL_L3_ARP);
	else if (acl_eq("ipv4"))
		acl_info(ACL_L3_IPV4);
	else if (acl_eq("ipv6"))
		acl_info(ACL_L3_IPV6);
	else
		acl_pm = ACL_INFO_L4;
	if (acl_pm == ACL_INFO_L3)
		return 1;
	if (acl_eq("icmp"))
		acl_info(ACL_L4_ICMP);
	else if (acl_eq("igmp"))
		acl_info(ACL_L4_IGMP);
	else if (acl_eq("tcp"))
		acl_info(ACL_L4_TCP);
	else if (acl_eq("udp"))
		acl_info(ACL_L4_UDP);
	else if (acl_eq("l4other"))
		acl_info(ACL_L4_OTHER);
	else
		acl_pm = ACL_INFO_CTAG;
	if (acl_pm == ACL_INFO_L4)
		return 1;
	if (acl_eq("tagged")) {
		acl_info(ACL_INFO_CTAG);
	} else if (acl_eq("untagged")) {
		acl_info(0);
	} else if (acl_eq("pppoe")) {
		acl_pm = ACL_INFO_PPPOE;
		acl_info(ACL_INFO_PPPOE);
	} else if (acl_eq("stagged")) {
		acl_pm = ACL_INFO_STAG;
		acl_info(ACL_INFO_STAG);
	} else {
		return 0;
	}
	return 1;
}

static uint8_t acl_match(void)
{
	if (acl_flag())
		return 1;
	if (acl_eq("dmac") || acl_eq("smac")) {
		acl_ft = acl_eq("dmac") ? ACL_FT_DMAC0 : ACL_FT_SMAC0;
		if (!acl_macm())
			return ACL_BAD;
		for (acl_c = 0; acl_c < 3; acl_c++) {
			acl_r = 4 - (acl_c << 1);
			acl_vk = acl_mac[acl_r];
			acl_vk <<= 8;
			acl_vk |= acl_mac[acl_r + 1];
			acl_mk = acl_mmask[acl_r];
			acl_mk <<= 8;
			acl_mk |= acl_mmask[acl_r + 1];
			acl_match_key(acl_ft + acl_c);
		}
		return 1;
	}
	if (acl_eq("ethertype")) {
		acl_max = 0xffff;
		acl_ft = ACL_FT_ETHERTYPE;
		if (!acl_hexm())
			return ACL_BAD;
		acl_key_set();
		return 1;
	}
	if (acl_eq("vlan") || acl_eq("svlan")) {
		acl_ft = acl_eq("vlan") ? ACL_FT_CTAG : ACL_FT_STAG;
		acl_rq.type = acl_ft == ACL_FT_CTAG ? ACL_VID_CVID : ACL_VID_SVID;
		acl_kind = ACL_RNG_VID;
		acl_max = 4095;
		acl_m = 0x0fff;
		return acl_range() && acl_exact() ? 1 : ACL_BAD;
	}
	acl_ft = ACL_BAD;
	if (acl_eq("pri") || acl_eq("cfi"))
		acl_ft = ACL_FT_CTAG;
	else if (acl_eq("spri") || acl_eq("sdei"))
		acl_ft = ACL_FT_STAG;
	if (acl_ft != ACL_BAD) {
		acl_kind = acl_eq("cfi") || acl_eq("sdei");
		acl_max = acl_kind ? 1 : 7;
		if (!acl_val())
			return ACL_BAD;
		acl_lo = acl_n << (acl_kind ? 12 : 13);
		acl_m = acl_kind ? 0x1000 : 0xe000;
		acl_key_set();
		return 1;
	}
	if (acl_eq("sip") || acl_eq("dip")) {
		acl_ft = acl_eq("sip") ? ACL_FT_SIP0 : ACL_FT_DIP0;
		acl_rq.type = acl_ft == ACL_FT_SIP0 ? ACL_IP_SIP : ACL_IP_DIP;
		return acl_ip() && acl_ip_key() ? 1 : ACL_BAD;
	}
	if (acl_eq("sip6") || acl_eq("dip6")) {
		acl_rq.type = acl_eq("sip6") ? ACL_IP_SIP6 : ACL_IP_DIP6;
		if (!acl_next() || !acl_hex())
			return ACL_BAD;
		acl_lo = acl_hi = acl_n;
		if (acl_sep('-')) {
			if (!acl_hex() || acl_n < acl_lo)
				return ACL_BAD;
			acl_hi = acl_n;
		}
		acl_kind = ACL_RNG_IP;
		return acl_end() && acl_rq_set(ACL_BAD) ? 1 : ACL_BAD;
	}
	if (acl_eq("proto")) {
		acl_max = 255;
		if (!acl_val())
			return ACL_BAD;
		acl_ft = ACL_FT_IPTOSPROTO;
		acl_lo = acl_n;
		acl_m = 0x00ff;
		acl_key_set();
		return 1;
	}
	if (acl_eq("tos")) {
		acl_max = 0xff;
		if (!acl_hexm())
			return ACL_BAD;
		acl_ft = ACL_FT_IPTOSPROTO;
		acl_lo <<= 8;
		acl_m <<= 8;
		acl_key_set();
		return 1;
	}
	if (acl_eq("sport") || acl_eq("dport")) {
		acl_ft = acl_eq("sport") ? ACL_FT_L4SPORT : ACL_FT_L4DPORT;
		acl_rq.type = acl_ft == ACL_FT_L4SPORT ? ACL_PORT_SPORT : ACL_PORT_DPORT;
		acl_kind = ACL_RNG_PORT;
		acl_max = 0xffff;
		acl_m = 0xffff;
		return acl_range() && acl_exact() ? 1 : ACL_BAD;
	}
	if (acl_eq("valid")) {
		acl_max = ACL_SELECTORS - 1;
		if (!acl_val())
			return ACL_BAD;
		acl_ft = ACL_FT_FIELD_VALID;
		acl_lo = acl_m = (uint16_t)1 << acl_n;
		acl_key_set();
		return 1;
	}
	if (acl_eq("field")) {
		acl_max = ACL_SELECTORS - 1;
		if (!acl_val())
			return ACL_BAD;
		acl_ft = ACL_FT_SEL0 + acl_n;
		acl_max = 0xffff;
		if (!acl_hexm())
			return ACL_BAD;
		acl_key_set();
		return 1;
	}
	return 0;
}

static uint8_t acl_set(uint16_t bit)
{
	if (acl_ctrl & bit)
		return 0;
	acl_ctrl |= bit;
	return 1;
}

static uint8_t acl_fwd(void)
{
	acl_n = acl_pm;
	acl_n <<= 4;
	acl_n |= acl_kind;
	acl_n <<= 17;
	acl_act[1] |= acl_n;
	return acl_set(ACL_ACT_FWD) ? 1 : ACL_BAD;
}

static uint8_t acl_fwd_action(void)
{
	acl_pm = 0;
	acl_kind = ACL_BAD;
	if (acl_eq("drop"))
		acl_kind = ACL_FWD_REDIRECT;
	else if (acl_eq("permit"))
		acl_kind = ACL_FWD_COPY;
	else if (acl_eq("trap"))
		acl_kind = ACL_FWD_TRAP;
	if (acl_kind == ACL_FWD_TRAP && acl_w + 1 < cmd_words_len) {
		acl_w++;
		if (acl_eq("ext"))
			acl_kind = ACL_FWD_TRAP_EXT;
		else if (acl_eq("both"))
			acl_kind = ACL_FWD_TRAP_BOTH;
		else if (!acl_eq("int"))
			acl_w--;
	}
	else if (acl_eq("cpu"))
		acl_kind = ACL_FWD_REDIRECT;
	if (acl_eq("cpu"))
		acl_pm = 1 << CPU_PORT;
	if (acl_kind != ACL_BAD)
		return acl_fwd();
	if (acl_eq("redirect"))
		acl_kind = ACL_FWD_REDIRECT;
	else if (acl_eq("copy"))
		acl_kind = ACL_FWD_COPY;
	else if (acl_eq("mirror"))
		acl_kind = ACL_FWD_MIRROR;
	else if (acl_eq("isolate"))
		acl_kind = ACL_FWD_COPY | 8;
	if (acl_kind != ACL_BAD)
		return acl_next() && acl_ports() ? acl_fwd() : ACL_BAD;
	if ((acl_ctrl & 0xff) || acl_vlan || acl_tag)
		return 0;
	acl_p = cmd_words_b[acl_w];
	acl_c = acl_port();
	if (acl_c == ACL_BAD || !acl_end())
		return 0;
	acl_kind = ACL_FWD_REDIRECT;
	acl_pm = (uint16_t)1 << acl_c;
	return acl_fwd();
}

static uint8_t acl_svlan_action(void)
{
	acl_kind = ACL_BAD;
	if (acl_eq("setsvlan"))
		acl_kind = 0;
	else if (acl_eq("outsvlan"))
		acl_kind = 1;
	else if (acl_eq("svidfromcvid"))
		acl_kind = 2;
	if (acl_kind == ACL_BAD)
		return 0;
	if (acl_svlan)
		return ACL_BAD;
	acl_svlan = 1;
	acl_n = 0;
	acl_max = 4095;
	if (acl_kind != 2 && !acl_val())
		return ACL_BAD;
	acl_n <<= 2;
	acl_n |= acl_kind;
	acl_n <<= 18;
	acl_act[0] |= acl_n;
	return 1;
}

static uint8_t acl_vlan_action(void)
{
	if (acl_eq("cvidfromsvid")) {
		if (acl_vlan)
			return ACL_BAD;
		acl_vlan = 1;
		acl_act[0] |= 2;
		return 1;
	}
	if (acl_eq("setvlan") || acl_eq("outvlan")) {
		acl_kind = acl_eq("outvlan");
		acl_max = 4095;
		if (acl_vlan || !acl_val())
			return ACL_BAD;
		acl_vlan = 1;
		acl_n <<= 4;
		acl_act[0] |= acl_kind;
		acl_act[0] |= acl_n;
		return 1;
	}
	acl_kind = ACL_BAD;
	if (acl_eq("untag"))
		acl_kind = 0;
	else if (acl_eq("tag"))
		acl_kind = 1;
	else if (acl_eq("keeptag"))
		acl_kind = 2;
	else if (acl_eq("keepremark"))
		acl_kind = 3;
	if (acl_kind == ACL_BAD)
		return 0;
	if (acl_tag)
		return ACL_BAD;
	acl_tag = 1;
	acl_n = acl_kind;
	acl_n <<= 16;
	acl_act[0] |= acl_n;
	return 1;
}

static uint8_t acl_action(void)
{
	acl_r = acl_fwd_action();
	if (acl_r)
		return acl_r;
	acl_r = acl_vlan_action();
	if (acl_r)
		return acl_r;
	acl_r = acl_svlan_action();
	if (acl_r)
		return acl_r;
	if (acl_eq("priority")) {
		acl_max = 7;
		if (!acl_val() || !acl_set(ACL_ACT_PRI))
			return ACL_BAD;
		acl_act[1] |= acl_n;
		return 1;
	}
	if (acl_eq("pcp") || acl_eq("dscp")) {
		acl_kind = acl_eq("dscp");
		acl_max = acl_kind ? 63 : 7;
		if (!acl_val() || !acl_set(ACL_ACT_RMK))
			return ACL_BAD;
		acl_n <<= 4;
		if (acl_kind)
			acl_act[1] |= 8;
		acl_act[1] |= acl_n;
		return 1;
	}
	if (acl_eq("police")) {
		if (!acl_next() || !acl_set(ACL_ACT_POLIC_LOG))
			return ACL_BAD;
		for (acl_c = 0; acl_c < 3; acl_c++) {
			if (acl_c && !acl_sep(','))
				break;
			if (!acl_dec() || acl_n >= ACL_METERS)
				return ACL_BAD;
			acl_pol[acl_c] = acl_n;
		}
		if (!acl_end())
			return ACL_BAD;
		acl_n = acl_pol[0];
		acl_n <<= 10;
		acl_act[1] |= acl_n;
		if (acl_c > 1) {
			if (acl_vlan)
				return ACL_BAD;
			acl_vlan = 1;
			acl_n = acl_pol[1];
			acl_n <<= 4;
			acl_n |= 3;
			acl_act[0] |= acl_n;
		}
		if (acl_c > 2) {
			if (acl_svlan)
				return ACL_BAD;
			acl_svlan = 1;
			acl_n = acl_pol[2];
			acl_n <<= 2;
			acl_n |= 3;
			acl_n <<= 18;
			acl_act[0] |= acl_n;
		}
		return 1;
	}
	if (acl_eq("count")) {
		acl_kind = acl_eq("count");
		acl_max = acl_kind ? ACL_COUNTERS - 1 : ACL_METERS - 1;
		if (!acl_val() || !acl_set(ACL_ACT_POLIC_LOG))
			return ACL_BAD;
		acl_n |= (uint16_t)acl_kind << 6;
		acl_n <<= 10;
		acl_act[1] |= acl_n;
		return 1;
	}
	if (acl_eq("interrupt")) {
		if (acl_intr & 1)
			return ACL_BAD;
		acl_intr |= 1;
		acl_act[1] |= 0x80000000;
		return 1;
	}
	if (acl_eq("gpio")) {
		acl_max = ACL_GPIO_PINS - 1;
		if ((acl_intr & 2) || !acl_val())
			return ACL_BAD;
		acl_intr |= 2;
		acl_n |= 0x10;
		acl_act[2] |= acl_n;
		return 1;
	}
	if (acl_eq("bypass")) {
		if (!acl_next())
			return ACL_BAD;
		acl_kind = 0;
		if (acl_eq("storm"))
			acl_kind = 0x20;
		else if (acl_eq("stp"))
			acl_kind = 0x40;
		else if (acl_eq("vlan"))
			acl_kind = 0x80;
		if (!acl_kind || (acl_byp & acl_kind))
			return ACL_BAD;
		acl_byp |= acl_kind;
		acl_act[2] |= acl_kind;
		acl_ctrl |= ACL_ACT_BYPASS;
		return 1;
	}
	return 0;
}

static uint8_t acl_in_ports(void)
{
	for (; acl_w < cmd_words_len; acl_w++) {
		acl_p = cmd_words_b[acl_w];
		acl_c = acl_port();
		if (acl_c == ACL_BAD || !acl_end())
			return 0;
		acl_in_pmask |= (uint16_t)1 << acl_c;
	}
	return 1;
}

static uint8_t acl_rule_cmd(void)
{
	acl_p = cmd_words_b[1];
	if (!acl_dec() || !acl_end() || !acl_n || acl_n > ACL_RULES || cmd_words_len < 3)
		return 0;
	acl_rn = acl_n - 1;
	acl_w = 2;
	if (cmd_words_len == 3 && acl_eq("off")) {
		acl_rule_clear(acl_rn);
		return 1;
	}
	acl_match_begin();
	acl_vlan = acl_tag = acl_cont = acl_byp = acl_svlan = acl_intr = 0;
	if (acl_eq("and")) {
		acl_cont = 1;
		acl_w++;
	} else if (acl_eq("not")) {
		acl_ctrl = ACL_ACT_NOT;
		acl_w++;
	}
	for (; acl_w < cmd_words_len; acl_w++) {
		acl_r = acl_match();
		if (acl_r == ACL_BAD)
			return 0;
		if (!acl_r)
			break;
	}
	if (!acl_cont) {
		for (; acl_w < cmd_words_len; acl_w++) {
			acl_r = acl_action();
			if (acl_r == ACL_BAD)
				return 0;
			if (!acl_r)
				break;
		}
		if (acl_vlan || acl_tag) {
			acl_act[0] |= (acl_vlan ? (acl_tag ? 1 : 0) : 2) << 2;
			acl_ctrl |= ACL_ACT_CVLAN;
		}
		if (acl_svlan)
			acl_ctrl |= ACL_ACT_SVLAN;
		if (acl_intr)
			acl_ctrl |= ACL_ACT_INT;
		if (!(acl_ctrl & 0xff))
			return 0;
	}
	if (!acl_in_ports())
		return 0;
	acl_r = acl_rule_set(acl_rn);
	if (acl_r == ACL_ERR_TEMPLATE)
		print_string("acl: these fields share no template, continue with acl <n+1> and ...\n");
	else if (acl_r == ACL_ERR_RANGE)
		print_string("acl: no free range entry\n");
	return !acl_r;
}

static void acl_print_reg(void)
{
	reg_read_m(acl_ra);
	print_long(((uint32_t)sfr_data[0] << 24) | ((uint32_t)sfr_data[1] << 16)
		   | ((uint16_t)sfr_data[2] << 8) | sfr_data[3]);
}

static void acl_show(void)
{
	for (acl_c = 0; acl_c < ACL_RULES; acl_c++) {
		if (!(acl_used[acl_c >> 3] & (1 << (acl_c & 7))))
			continue;
		print_string("rule ");
		itoa(acl_c + 1);
		print_string(": template ");
		itoa(acl_rule_tmpl[acl_c]);
		write_char('\n');
	}
	print_string("counters:");
	for (acl_c = 0; acl_c < ACL_COUNTERS; acl_c++) {
		acl_ra = RTL837X_ACL_LOG_DATA + ((uint16_t)acl_c << 2);
		reg_read_m(acl_ra);
		if (!(sfr_data[0] | sfr_data[1] | sfr_data[2] | sfr_data[3]))
			continue;
		write_char(' ');
		itoa(acl_c);
		write_char('=');
		acl_print_reg();
	}
	print_string("\nhit: ");
	acl_ra = RTL837X_ACL_HIT;
	acl_print_reg();
	write_char(' ');
	acl_ra = RTL837X_ACL_HIT + 4;
	acl_print_reg();
	print_string("\nmeters exceeded: ");
	acl_ra = RTL837X_METER_EXCEED;
	acl_print_reg();
	reg_read_m(acl_ra);
	reg_write_m(acl_ra);
	write_char(' ');
	acl_ra = RTL837X_METER_EXCEED + 4;
	acl_print_reg();
	reg_read_m(acl_ra);
	reg_write_m(acl_ra);
	write_char('\n');
}

static __code const char * __code const acl_formats[] = {
	"off", "raw", "llc", "ipv4", "arp", "ipv6", "ip", "l4"
};

static uint8_t acl_bit(void)
{
	reg_read_m(acl_ra);
	acl_c = 3 - ((acl_r >> 3) & 3);
	if (acl_kind)
		sfr_data[acl_c] |= 1 << (acl_r & 7);
	else
		sfr_data[acl_c] &= ~(1 << (acl_r & 7));
	reg_write_m(acl_ra);
	return 1;
}

static uint8_t acl_global_cmd(void)
{
	acl_w = 1;
	if (acl_eq("show"))
		return cmd_words_len == 2 ? (acl_show(), 1) : 0;
	if (acl_eq("counter")) {
		if (cmd_words_len == 3 && acl_next() && acl_eq("reset")) {
			acl_counter_reset();
			return 1;
		}
		acl_max = ACL_COUNTERS - 1;
		if (cmd_words_len != 5 || !acl_val() || !acl_next())
			return 0;
		acl_r = acl_n >> 1;
		acl_ra = acl_eq("mode") ? RTL837X_ACL_LOG_TYPE : RTL837X_ACL_LOG_MODE;
		if (acl_ra == RTL837X_ACL_LOG_MODE && !acl_eq("width"))
			return 0;
		if (!acl_next())
			return 0;
		if (acl_ra == RTL837X_ACL_LOG_TYPE) {
			acl_kind = acl_eq("bytes");
			if (!acl_kind && !acl_eq("packets"))
				return 0;
		} else {
			acl_kind = acl_eq("64");
			if (!acl_kind && !acl_eq("32"))
				return 0;
		}
		return acl_bit();
	}
	if (acl_eq("meter")) {
		acl_kind = 0;
		if (cmd_words_len == 7) {
			acl_w = 6;
			acl_kind = acl_eq("ifg");
			acl_w = 1;
		}
		if (cmd_words_len != 6 + acl_kind)
			return 0;
		acl_intr = acl_kind;
		acl_max = ACL_METERS - 1;
		if (!acl_val())
			return 0;
		acl_r = acl_n;
		acl_max = 0xffffff;
		if (!acl_val())
			return 0;
		acl_lo = acl_n;
		if (!acl_next())
			return 0;
		acl_kind = acl_eq("pps");
		if (!acl_kind && !acl_eq("kbps"))
			return 0;
		acl_max = 0xfffffff;
		if (!acl_val())
			return 0;
		acl_ra = RTL837X_METER_BURST + ((uint16_t)acl_r << 2);
		acl_rv = acl_n;
		acl_reg();
		acl_ra = RTL837X_METER_RATE + ((uint16_t)acl_r << 2);
		acl_rv = acl_lo;
		acl_reg();
		acl_ra = RTL837X_METER_MODE + ((acl_r >> 5) << 2);
		acl_bit();
		acl_kind = acl_intr;
		acl_ra = RTL837X_METER_IFG + ((acl_r >> 5) << 2);
		return acl_bit();
	}
	if (acl_eq("field")) {
		acl_max = ACL_SELECTORS - 1;
		if (cmd_words_len != 5 || !acl_val() || !acl_next())
			return 0;
		acl_r = acl_n;
		for (acl_kind = 0; acl_kind < 8; acl_kind++)
			if (acl_eq(acl_formats[acl_kind]))
				break;
		acl_max = 255;
		if (acl_kind == 8 || !acl_val())
			return 0;
		acl_ra = RTL837X_ACL_FIELD_SEL + ((uint16_t)acl_r << 2);
		acl_rv = acl_n;
		acl_rv <<= 3;
		acl_rv |= acl_kind;
		acl_reg();
		return 1;
	}
	if (acl_eq("gpio")) {
		if (cmd_words_len != 4 || !acl_next())
			return 0;
		if (acl_eq("polarity")) {
			if (!acl_next())
				return 0;
			acl_kind = acl_eq("high");
			if (!acl_kind && !acl_eq("low"))
				return 0;
			REG_SET(RTL837X_ACL_GPIO_CTRL, acl_kind);
			return 1;
		}
		acl_w--;
		acl_max = ACL_GPIO_PINS - 1;
		if (!acl_val() || !acl_next())
			return 0;
		acl_r = acl_n;
		acl_kind = acl_eq("on");
		if (!acl_kind && !acl_eq("off"))
			return 0;
		acl_ra = RTL837X_IO_MUX_SEL_2;
		return acl_bit();
	}
	if (acl_eq("default")) {
		if (!acl_next())
			return 0;
		if (acl_eq("permit") && cmd_words_len == 3) {
			acl_unmatch_drop = 0;
			acl_unmatch_set();
			return 1;
		}
		if (!acl_eq("drop") || cmd_words_len < 4)
			return 0;
		acl_in_pmask = 0;
		acl_w++;
		if (!acl_in_ports())
			return 0;
		acl_unmatch_drop = acl_in_pmask;
		acl_unmatch_set();
		return 1;
	}
	return ACL_BAD;
}

void acl_cmd(void) __banked
{
	acl_r = 0;
	if (cmd_words_len >= 2) {
		acl_r = acl_global_cmd();
		if (acl_r == ACL_BAD)
			acl_r = acl_rule_cmd();
	}
	if (acl_r)
		return;
	err_status = ERR_INVALID_ARGUMENT;
	print_string("Error: acl <1-64> [not|and] <match>... <action>... [<port>...] | acl <1-64> off"
		     " | acl meter <0-63> <rate> kbps|pps <burst> [ifg] | acl field <0-15> <format> <offset>"
		     " | acl default permit|drop <port>... | acl counter reset|<n> mode bytes|packets|<n> width 32|64"
		     " | acl gpio <0-3> on|off | acl gpio polarity high|low"
		     " | acl show\n");
}

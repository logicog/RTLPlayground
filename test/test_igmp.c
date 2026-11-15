#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "machine.h"
#include "uip.h"
#include "rtl837x_regs.h"
#include "rtl837x_igmp.h"
#include "rtl837x_port.h"

uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
uint16_t uip_len;
const struct machine machine = { .log_to_phys_port = {5, 1, 2, 3, 4, 6, 7, 8, 9} };
struct machine_runtime machine_detected = { .isRTL8373 = 1 };
uint8_t igmpEnabled = 1;
uint8_t cpuPort = CPU_LOGICAL_PORT;
uint8_t sfr_data[4];
volatile uint8_t SFR_DATA_24, SFR_DATA_16, SFR_DATA_8, SFR_DATA_0;

static int verbose = 0;
void print_string(const char *s) { if (verbose) fputs(s, stdout); }
void print_byte(uint8_t b) { if (verbose) printf("%02x", b); }
void print_short(uint16_t v) { if (verbose) printf("%04x", v); }
void write_char(char c) { if (verbose) putchar(c); }
void print_sfr_data(void) { }

static int tbl_writes;
static uint8_t cap_b[4], cap_c[4];
static uint32_t lm_act;
void reg_read_m(uint16_t reg) { (void)reg; memset(sfr_data, 0, sizeof(sfr_data)); }
void reg_write_m(uint16_t reg) { (void)reg; }
void reg_write(uint16_t reg)
{
	if (reg == RTL837X_TBL_CTRL)
		tbl_writes++;
	if (reg == RTL837X_IPV4_PORT_MC_LM_ACT)
		lm_act = ((uint32_t)SFR_DATA_24 << 24) | ((uint32_t)SFR_DATA_16 << 16) | ((uint32_t)SFR_DATA_8 << 8) | SFR_DATA_0;
	if (reg == RTL837x_TBL_DATA_IN_B) {
		cap_b[0] = SFR_DATA_24; cap_b[1] = SFR_DATA_16; cap_b[2] = SFR_DATA_8; cap_b[3] = SFR_DATA_0;
	}
	if (reg == RTL837x_TBL_DATA_IN_C) {
		cap_c[0] = SFR_DATA_24; cap_c[1] = SFR_DATA_16; cap_c[2] = SFR_DATA_8; cap_c[3] = SFR_DATA_0;
	}
}
static int lut = -1;
void reg_bit_set(uint16_t reg, char bit) { if (reg == RTL837X_L2_CTRL && bit == L2_CTRL_LUT_IPMC_HASH) lut = 1; }
void reg_bit_clear(uint16_t reg, char bit) { if (reg == RTL837X_L2_CTRL && bit == L2_CTRL_LUT_IPMC_HASH) lut = 0; }

static uint16_t pvid[16];
uint16_t port_pvid_get(uint8_t port) { return pvid[port]; }

static uint16_t vlan_members(uint16_t vid)
{
	switch (vid) {
	case 1: case 2: case 7: case 20: return 0x3ff;
	case 30: return 0x3ff & ~PORT_BIT(2);
	case 40: return 0x3ff;
	default: return 0;
	}
}
int8_t vlan_get(uint16_t vid)
{
	uint16_t m = vlan_members(vid);
	memset(sfr_data, 0, sizeof(sfr_data));
	if (vid >= 0xfff)
		return -1;
	sfr_data[0] = m && vid != 40 ? 0x02 : 0;
	sfr_data[2] = m >> 8;
	sfr_data[3] = m;
	return 0;
}

bool stp_enabled;
static uint8_t stp_state[16];
uint8_t stp_port_state(uint8_t port) { return stp_state[port]; }

static uint16_t lag_members[4];
static uint16_t link_up = 0x3ff;
uint16_t port_link_mask(void) { return link_up; }
static uint16_t iso_block;
uint16_t port_isolation_get(uint8_t port) { (void)port; return (uint16_t)~iso_block; }
uint16_t port_lag_members_get(uint8_t lag) { return lag_members[lag]; }
uint8_t port_lag_of(uint8_t port)
{
	for (uint8_t lag = 0; lag < 4; lag++)
		if ((lag_members[lag] >> port) & 1)
			return lag;
	return PORT_LAG_NONE;
}

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, msg); fails++; } } while (0)

struct wire_in {
	uint8_t dst[6];
	uint8_t src[6];
	struct rtl_tag rtl_tag;
	uint16_t ethertype;
	uint8_t ip[24];
	uint8_t igmp[16];
};

struct wire_out {
	uint8_t dst[6];
	uint8_t src[6];
	struct rtl_tag rtl_tag;
	struct vlan_tag vlan_tag;
	uint16_t ethertype;
	uint8_t ip[24];
	uint8_t igmp[16];
};

static struct wire_in in;
static uint16_t rp;

static void build(uint8_t port, const uint8_t dip[4], uint8_t type, const uint8_t grp[4])
{
	memset(&in, 0, sizeof(in));
	in.dst[0] = 0x01; in.dst[1] = 0x00; in.dst[2] = 0x5e;
	in.dst[3] = dip[1] & 0x7f; in.dst[4] = dip[2]; in.dst[5] = dip[3];
	memcpy(in.src, "\x02\xaa\xbb\xcc\xdd\xee", 6);
	in.rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	in.rtl_tag.version = RTL_FRAME_TAG_VERSION;
	in.rtl_tag.pmask = HTONS(port);
	in.ethertype = HTONS(0x0800);
	in.ip[0] = 0x46;
	in.ip[9] = IGMP_PROTO;
	in.ip[12] = 10; in.ip[15] = 1;
	memcpy(&in.ip[16], dip, 4);
	in.ip[20] = 0x94; in.ip[21] = 0x04;
	in.igmp[0] = type;
	if (type == IGMP_V3_REPORT) {
		in.igmp[7] = 1;
		in.igmp[8] = IGMP_V3_TO_EXCLUDE;
		memcpy(&in.igmp[12], grp, 4);
	} else {
		memcpy(&in.igmp[4], grp, 4);
	}
	memset(uip_buf, 0xa5, sizeof(uip_buf));
	memcpy(uip_buf, &in, sizeof(in));
	uip_len = sizeof(in);
}

static void check_forwarded(uint8_t port, uint16_t vid, const char *what)
{
	struct wire_out *o = (struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE];
	char msg[96];

	snprintf(msg, sizeof(msg), "%s: length grows by the VLAN tag", what);
	CHECK(uip_len == sizeof(in) + VLAN_TAG_SIZE, msg);
	snprintf(msg, sizeof(msg), "%s: addresses kept", what);
	CHECK(!memcmp(o->dst, in.dst, 6) && !memcmp(o->src, in.src, 6), msg);
	snprintf(msg, sizeof(msg), "%s: CPU tag for TX", what);
	CHECK(o->rtl_tag.tag == HTONS(RTL_FRAME_TAG_ID) && o->rtl_tag.version == RTL_FRAME_TAG_VERSION
	      && o->rtl_tag.reason == 0 && o->rtl_tag.flags == HTONS(RTL_TAG_LEARN_DIS), msg);
	snprintf(msg, sizeof(msg), "%s: sent to the right ports", what);
	CHECK(o->rtl_tag.pmask == HTONS(((in.igmp[0] == IGMP_QUERY || !rp ? PMASK_9 : rp) & ~PORT_BIT(port))), msg);
	snprintf(msg, sizeof(msg), "%s: tagged with ingress PVID", what);
	CHECK(o->vlan_tag.svlan == HTONS(0x8100) && o->vlan_tag.vlan == HTONS(vid), msg);
	snprintf(msg, sizeof(msg), "%s: IP and IGMP unchanged", what);
	CHECK(o->ethertype == in.ethertype && !memcmp(o->ip, in.ip, sizeof(in.ip))
	      && !memcmp(o->igmp, in.igmp, sizeof(in.igmp)), msg);
}

static struct wire_out tin;

static void build_tagged(uint8_t port, uint16_t tci, const uint8_t dip[4], uint8_t type, const uint8_t grp[4])
{
	build(port, dip, type, grp);
	memset(&tin, 0, sizeof(tin));
	memcpy(tin.dst, in.dst, 6);
	memcpy(tin.src, in.src, 6);
	tin.rtl_tag = in.rtl_tag;
	tin.vlan_tag.svlan = HTONS(0x8100);
	tin.vlan_tag.vlan = HTONS(tci);
	tin.ethertype = in.ethertype;
	memcpy(tin.ip, in.ip, sizeof(tin.ip));
	memcpy(tin.igmp, in.igmp, sizeof(tin.igmp));
	memset(uip_buf, 0xa5, sizeof(uip_buf));
	memcpy(uip_buf, &tin, sizeof(tin));
	uip_len = sizeof(tin);
}

static void check_forwarded_tagged(uint8_t port, const char *what)
{
	struct wire_out *o = (struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE];
	char msg[96];

	snprintf(msg, sizeof(msg), "%s: length unchanged", what);
	CHECK(uip_len == sizeof(tin), msg);
	snprintf(msg, sizeof(msg), "%s: addresses kept", what);
	CHECK(!memcmp(o->dst, tin.dst, 6) && !memcmp(o->src, tin.src, 6), msg);
	snprintf(msg, sizeof(msg), "%s: CPU tag for TX", what);
	CHECK(o->rtl_tag.tag == HTONS(RTL_FRAME_TAG_ID) && o->rtl_tag.flags == HTONS(RTL_TAG_LEARN_DIS)
	      && o->rtl_tag.pmask == HTONS(((tin.igmp[0] == IGMP_QUERY || !rp ? PMASK_9 : rp) & ~PORT_BIT(port))), msg);
	snprintf(msg, sizeof(msg), "%s: VLAN tag of the frame kept", what);
	CHECK(o->vlan_tag.svlan == tin.vlan_tag.svlan && o->vlan_tag.vlan == tin.vlan_tag.vlan, msg);
	snprintf(msg, sizeof(msg), "%s: IP and IGMP unchanged", what);
	CHECK(o->ethertype == tin.ethertype && !memcmp(o->ip, tin.ip, sizeof(tin.ip))
	      && !memcmp(o->igmp, tin.igmp, sizeof(tin.igmp)), msg);
}

static const uint8_t all_hosts[4] = {224, 0, 0, 1};
static const uint8_t v3_reports[4] = {224, 0, 0, 22};
static const uint8_t grp[4] = {239, 1, 2, 4};
static const uint8_t grp2[4] = {239, 1, 2, 5};
static const uint8_t none[4] = {0, 0, 0, 0};
static const uint8_t alias[4] = {0, 0, 0, 251};
static const uint8_t unicast[4] = {10, 1, 2, 4};
static const uint8_t mdns[4] = {224, 0, 0, 251};

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "-v"))
		verbose = 1;
	for (int i = 0; i < 16; i++)
		pvid[i] = 1;
	pvid[4] = 20;

	build(8, all_hosts, 0x11, none);
	CHECK(igmp_packet_handler() == 1, "query: handled");
	check_forwarded(8, 1, "general query");
	rp = PORT_BIT(8);

	build(4, v3_reports, IGMP_V3_REPORT, grp);
	tbl_writes = 0;
	CHECK(igmp_packet_handler() == 1, "v3 report: handled");
	CHECK(tbl_writes > 0, "v3 report: table updated");
	check_forwarded(4, 20, "v3 report");

	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1, "v2 report: handled");
	check_forwarded(2, 1, "v2 report");

	build(2, all_hosts, IGMP_V2_LEAVE, grp);
	CHECK(igmp_packet_handler() == 1, "v2 leave: handled");
	check_forwarded(2, 1, "v2 leave without entry");

	build(2, all_hosts, IGMP_V2_REPORT, all_hosts);
	CHECK(igmp_packet_handler() == 1, "local group: handled");
	check_forwarded(2, 1, "report for 224.0.0.x");

	machine_detected.isRTL8373 = 0;
	build(4, all_hosts, 0x11, none);
	CHECK(igmp_packet_handler() == 1, "8372 query: handled");
	CHECK(((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS((PMASK_6 & ~PORT_BIT(4))), "8372: only its ports");
	rp |= PORT_BIT(4);
	machine_detected.isRTL8373 = 1;

	build(15, grp, IGMP_V2_REPORT, grp);
	tbl_writes = 0;
	CHECK(igmp_packet_handler() == 1 && uip_len == 0 && tbl_writes == 0, "invalid ingress port: nothing sent, nothing learned");

	build(8, all_hosts, 0x11, none);
	uip_len = UIP_CONF_BUFFER_ALLOC_SIZE - RTL_FRAME_DESC_SIZE;
	CHECK(igmp_packet_handler() == 1 && uip_len == 0, "oversize frame: nothing sent");

	stp_enabled = true;
	memset(stp_state, 3, sizeof(stp_state));
	stp_state[2] = 1;
	build(2, grp, IGMP_V2_REPORT, grp);
	tbl_writes = 0;
	CHECK(igmp_packet_handler() == 1 && uip_len == 0, "STP: nothing sent from a blocked port");
	CHECK(tbl_writes == 0, "STP: nothing learned from a blocked port");
	build(8, all_hosts, 0x11, none);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PMASK_9 & ~(PORT_BIT(2) | PORT_BIT(8))), "STP: query not sent to a blocked port");
	stp_enabled = false;

	lag_members[1] = PORT_BIT(2) | PORT_BIT(3);
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1, "LAG: report handled");
	CHECK(((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS((rp & ~(PORT_BIT(2) | PORT_BIT(3)))), "LAG: not sent back into the ingress LAG");
	lag_members[1] = 0;
	lag_members[2] = PORT_BIT(5) | PORT_BIT(6);
	build(8, all_hosts, IGMP_QUERY, none);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PMASK_9 & ~(PORT_BIT(6) | PORT_BIT(8))), "LAG: sent on one member only");
	link_up &= ~PORT_BIT(5);
	build(8, all_hosts, IGMP_QUERY, none);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PMASK_9 & ~(PORT_BIT(5) | PORT_BIT(8))), "LAG: sent on a member with link");
	link_up = 0x3ff;
	lag_members[2] = 0;

	build(8, all_hosts, 0x11, none);
	uip_buf[offsetof(struct wire_in, ip) + 9] = 17;
	CHECK(igmp_packet_handler() == 0 && uip_len == sizeof(in), "not IGMP: left to the caller");

	build_tagged(8, 0xa002, all_hosts, 0x11, none);
	CHECK(igmp_packet_handler() == 1, "tagged query: handled");
	check_forwarded_tagged(8, "tagged query");

	build_tagged(2, 0x0002, grp, IGMP_V2_REPORT, grp);
	tbl_writes = 0;
	CHECK(igmp_packet_handler() == 1, "tagged v2 report: handled");
	CHECK(tbl_writes > 0, "tagged v2 report: table updated");
	check_forwarded_tagged(2, "tagged v2 report");

	build_tagged(4, 0x0014, v3_reports, IGMP_V3_REPORT, grp);
	CHECK(igmp_packet_handler() == 1, "tagged v3 report: handled");
	check_forwarded_tagged(4, "tagged v3 report");

	build_tagged(8, 0x0002, all_hosts, 0x11, none);
	uip_buf[offsetof(struct wire_out, ip) + 9] = 17;
	CHECK(igmp_packet_handler() == 0 && uip_len == sizeof(tin), "tagged, not IGMP: left to the caller");

	build_tagged(8, 0x0002, all_hosts, 0x11, none);
	uip_len = UIP_CONF_BUFFER_ALLOC_SIZE - RTL_FRAME_DESC_SIZE;
	CHECK(igmp_packet_handler() == 1 && uip_len == UIP_CONF_BUFFER_ALLOC_SIZE - RTL_FRAME_DESC_SIZE,
	      "tagged frame that fits: sent");
	build_tagged(8, 0x0002, all_hosts, 0x11, none);
	uip_len = UIP_CONF_BUFFER_ALLOC_SIZE - RTL_FRAME_DESC_SIZE + 1;
	CHECK(igmp_packet_handler() == 1 && uip_len == 0, "tagged oversize frame: nothing sent");

#define DIP_OK(g) (cap_b[1] == (g)[1] && cap_b[2] == (g)[2] && cap_b[3] == (g)[3] && (cap_b[0] & 0x0f) == ((g)[0] & 0x0f))
#define PM() ((uint16_t)(cap_b[0] >> 6) | ((uint16_t)cap_c[3] << 2))
#define CLR() do { memset(cap_b, 0, 4); memset(cap_c, 0, 4); } while (0)
	CLR(); build(2, grp, IGMP_V2_REPORT, grp); igmp_packet_handler();
	CHECK(DIP_OK(grp) && PM() == PORT_BIT(2), "untagged v2: group and port in table");
	CLR(); build(4, v3_reports, IGMP_V3_REPORT, grp); igmp_packet_handler();
	CHECK(DIP_OK(grp) && PM() == PORT_BIT(4), "untagged v3: group and port in table");
	CLR(); build_tagged(2, 2, grp, IGMP_V2_REPORT, grp); igmp_packet_handler();
	CHECK(DIP_OK(grp) && PM() == PORT_BIT(2), "tagged v2: group and port in table");
	CLR(); build_tagged(4, 0x14, v3_reports, IGMP_V3_REPORT, grp); igmp_packet_handler();
	CHECK(DIP_OK(grp) && PM() == PORT_BIT(4), "tagged v3: group and port in table");

#define REC(i) (offsetof(struct wire_in, igmp) + 8 + (i))
#define RECORDS(n) (uip_buf[offsetof(struct wire_in, igmp) + 7] = (n))
#define ADD(i, t, g) do { memset(&uip_buf[REC(i)], 0, 8); uip_buf[REC(i)] = (t); memcpy(&uip_buf[REC(i) + 4], (g), 4); } while (0)
	CLR(); build(4, v3_reports, IGMP_V3_REPORT, grp); uip_buf[REC(0)] = IGMP_V3_IS_EXCLUDE; igmp_packet_handler();
	CHECK(DIP_OK(grp) && PM() == PORT_BIT(4), "v3 current state exclude: learned");
	CLR(); build(4, v3_reports, IGMP_V3_REPORT, grp); uip_buf[REC(0)] = IGMP_V3_ALLOW_NEW; igmp_packet_handler();
	CHECK(DIP_OK(grp) && PM() == PORT_BIT(4), "v3 allow new sources: learned");
	CLR(); build(4, v3_reports, IGMP_V3_REPORT, grp); uip_buf[REC(0)] = IGMP_V3_IS_INCLUDE; uip_buf[REC(3)] = 1; uip_len += 4; igmp_packet_handler();
	CHECK(DIP_OK(grp) && PM() == PORT_BIT(4), "v3 include with a source: learned");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp); uip_buf[REC(0)] = IGMP_V3_IS_INCLUDE; igmp_packet_handler();
	CHECK(tbl_writes == 1, "v3 include without sources: leave, nothing written");
	CLR(); tbl_writes = 0; build(2, grp, IGMP_V2_REPORT, alias); igmp_packet_handler();
	CHECK(tbl_writes == 0, "group that the table would key as 224.0.0.x: ignored");
	CLR(); tbl_writes = 0; build(2, grp, IGMP_V2_REPORT, unicast); igmp_packet_handler();
	CHECK(tbl_writes == 0, "group outside 224.0.0.0/4: ignored");
	CLR(); tbl_writes = 0; build(2, grp, IGMP_V2_REPORT, mdns); igmp_packet_handler();
	CHECK(tbl_writes == 0, "group in 224.0.0.0/24: ignored");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp); uip_buf[REC(0)] = 6; igmp_packet_handler();
	CHECK(tbl_writes == 0, "v3 block old sources: ignored");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp); RECORDS(2);
	ADD(8, IGMP_V3_IS_EXCLUDE, grp2); uip_len += 8; igmp_packet_handler();
	CHECK(DIP_OK(grp2) && tbl_writes == 4, "v3 two records: both learned");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp2); RECORDS(2);
	ADD(8, IGMP_V3_IS_EXCLUDE, grp); uip_len += 8; igmp_packet_handler();
	CHECK(DIP_OK(grp) && tbl_writes == 4, "v3 two records, other order: both learned");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp); RECORDS(2); uip_buf[REC(0)] = IGMP_V3_IS_INCLUDE; uip_buf[REC(3)] = 1;
	ADD(12, IGMP_V3_IS_EXCLUDE, grp2); uip_len += 12; igmp_packet_handler();
	CHECK(DIP_OK(grp2) && tbl_writes == 4, "v3 record with a source, then another: both learned");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp); RECORDS(2); uip_buf[REC(0)] = 6;
	ADD(8, IGMP_V3_IS_EXCLUDE, grp2); uip_len += 8; igmp_packet_handler();
	CHECK(DIP_OK(grp2) && tbl_writes == 2, "v3 unknown record, then a known one: second learned");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp); RECORDS(2);
	ADD(8, IGMP_V3_IS_EXCLUDE, grp2); uip_len += 4; igmp_packet_handler();
	CHECK(DIP_OK(grp) && tbl_writes == 2, "v3 second record cut: only the first learned");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp); RECORDS(9); igmp_packet_handler();
	CHECK(DIP_OK(grp) && tbl_writes == 2, "v3 record count beyond the frame: only what is there");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp); uip_buf[REC(2)] = 0xff; uip_buf[REC(3)] = 0xff; igmp_packet_handler();
	CHECK(tbl_writes == 0, "v3 source count beyond any frame: nothing learned");
	CLR(); tbl_writes = 0; build(4, v3_reports, IGMP_V3_REPORT, grp); uip_len = offsetof(struct wire_in, igmp) + 7; igmp_packet_handler();
	CHECK(tbl_writes == 0, "v3 report cut before the record count: nothing learned");

	CLR(); build_tagged(2, 2, grp, IGMP_V2_REPORT, grp);
	uip_buf[offsetof(struct wire_out, ip)] = 0x45;
	memmove(&uip_buf[offsetof(struct wire_out, ip) + 20], &uip_buf[offsetof(struct wire_out, ip) + 24], 16);
	uip_len -= 4;
	igmp_packet_handler();
	CHECK(DIP_OK(grp), "tagged v2, IHL 5: group in table");
	CLR(); build(2, grp, IGMP_V2_REPORT, grp);
	uip_buf[offsetof(struct wire_in, ip)] = 0x45;
	memmove(&uip_buf[offsetof(struct wire_in, ip) + 20], &uip_buf[offsetof(struct wire_in, ip) + 24], 16);
	uip_len -= 4;
	igmp_packet_handler();
	CHECK(DIP_OK(grp), "untagged v2, IHL 5: group in table");

	build_tagged(15, 2, all_hosts, 0x11, none);
	CHECK(igmp_packet_handler() == 1 && uip_len == 0, "tagged, invalid port: nothing sent");
	build_tagged(8, 2, all_hosts, 0x11, none);
	uip_len = 23;
	CHECK(igmp_packet_handler() == 1 && uip_len == 0, "tagged, shorter than the tag: nothing sent");
	build(8, all_hosts, 0x11, none);
	uip_len = UIP_CONF_BUFFER_ALLOC_SIZE - RTL_FRAME_DESC_SIZE - VLAN_TAG_SIZE;
	CHECK(igmp_packet_handler() == 1 && uip_len == UIP_CONF_BUFFER_ALLOC_SIZE - RTL_FRAME_DESC_SIZE,
	      "untagged frame that fits: sent");
	build(8, all_hosts, 0x11, none);
	uip_len = UIP_CONF_BUFFER_ALLOC_SIZE - RTL_FRAME_DESC_SIZE - VLAN_TAG_SIZE + 1;
	CHECK(igmp_packet_handler() == 1 && uip_len == 0, "untagged one byte over: nothing sent");

	pvid[2] = 7;
	build_tagged(2, 0xa000, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1, "priority tag: handled");
	CHECK(((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->vlan_tag.vlan == HTONS(0xa007) && uip_len == sizeof(tin),
	      "priority tag: sent in the ingress PVID, priority kept");
	pvid[2] = 1;

	build_tagged(2, 0x0063, grp, IGMP_V2_REPORT, grp);
	tbl_writes = 0;
	CHECK(igmp_packet_handler() == 1 && uip_len == 0 && tbl_writes == 0, "VLAN that does not exist: dropped, not learned");
	build_tagged(2, 0x001e, grp, IGMP_V2_REPORT, grp);
	tbl_writes = 0;
	CHECK(igmp_packet_handler() == 1 && uip_len == 0 && tbl_writes == 0, "ingress port not in the VLAN: dropped, not learned");
	build_tagged(4, 0x001e, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && uip_len == sizeof(tin), "ingress port in the VLAN: sent");
	tbl_writes = 0;
	build_tagged(2, 40, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && uip_len == 0 && tbl_writes == 0, "VLAN entry not valid: dropped, not learned");

	build(2, grp, IGMP_V2_REPORT, grp);
	uip_len = 14;
	tbl_writes = 0;
	igmp_packet_handler();
	CHECK(tbl_writes == 0, "report cut before the group: not learned");
	build_tagged(2, 2, grp, IGMP_V2_REPORT, grp);
	uip_len = 30;
	tbl_writes = 0;
	igmp_packet_handler();
	CHECK(tbl_writes == 0, "tagged report cut before the group: not learned");
	build(2, grp, IGMP_V2_REPORT, grp);
	uip_buf[offsetof(struct wire_in, ip)] = 0x4f;
	tbl_writes = 0;
	igmp_packet_handler();
	CHECK(tbl_writes == 0, "IHL beyond the frame: not learned");

	igmp_setup();
	build(2, grp, IGMP_V2_REPORT, grp);
	tbl_writes = 0;
	CHECK(igmp_packet_handler() == 1 && tbl_writes > 0 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS((PMASK_9 & ~PORT_BIT(2))), "no router port known: report learned and flooded");
	build(6, all_hosts, IGMP_QUERY, none);
	memset(&uip_buf[offsetof(struct wire_in, ip) + 12], 0, 4);
	CHECK(igmp_packet_handler() == 1, "query from 0.0.0.0: handled");
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PORT_BIT(6)), "query from 0.0.0.0: querier port learned");
	build(6, all_hosts, IGMP_QUERY, none);
	CHECK(igmp_packet_handler() == 1 && uip_len == sizeof(in) + VLAN_TAG_SIZE, "query from the querier port: sent");
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PORT_BIT(6)), "report: sent only towards the querier");
	build(4, all_hosts, IGMP_V2_LEAVE, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PORT_BIT(6)), "leave: sent only towards the querier");
	build(6, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && uip_len == 0, "report from the router port: nothing sent");
	build(2, all_hosts, 0x13, none);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS((PMASK_9 & ~PORT_BIT(2))), "unknown IGMP type: flooded");
	build_tagged(7, 0x0063, all_hosts, IGMP_QUERY, none);
	CHECK(igmp_packet_handler() == 1 && uip_len == 0, "query in a foreign VLAN: dropped");
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PORT_BIT(6)), "query in a foreign VLAN: no router port learned");
	build_tagged(8, 30, all_hosts, IGMP_QUERY, none);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PMASK_9 & ~(PORT_BIT(2) | PORT_BIT(8))), "query: only to the members of its VLAN");
	link_up &= ~PORT_BIT(6);
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PORT_BIT(8)), "querier port without link: not used");
	link_up = 0x3ff;
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PORT_BIT(8)), "querier port without link: forgotten");
	igmp_enable();
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS((PMASK_9 & ~PORT_BIT(2))), "igmp on: router ports forgotten, report flooded");
	CHECK(lm_act == LOOKUP_MISS_FLOOD, "igmp on: unknown multicast flooded while no querier is known");
	CHECK(lut == 0, "igmp on: groups not looked up while no querier is known");
	build(6, all_hosts, IGMP_QUERY, none);
	igmp_packet_handler();
	CHECK(lm_act == LOOKUP_MISS_DROP_9, "first query: unknown multicast dropped");
	CHECK(lut == 1, "first query: groups looked up");
	lm_act = 1;
	build(6, all_hosts, IGMP_QUERY, none);
	igmp_packet_handler();
	CHECK(lm_act == 1, "query from a known querier port: lookup-miss action left alone");
	build(2, grp, IGMP_V2_REPORT, grp);
	igmp_packet_handler();
	CHECK(lm_act == 1, "report: lookup-miss action left alone");
	link_up &= ~PORT_BIT(6);
	build(2, grp, IGMP_V2_REPORT, grp);
	igmp_packet_handler();
	CHECK(lm_act == LOOKUP_MISS_FLOOD, "last querier port lost its link: unknown multicast flooded again");
	CHECK(lut == 0, "last querier port lost its link: groups not looked up");
	lut = -1;
	link_up = 0x3ff;
	lm_act = 1;
	igmp_setup();
	CHECK(lm_act == LOOKUP_MISS_FLOOD, "igmp off: unknown multicast flooded");
	CHECK(lut == 0, "igmp off: groups not looked up");
	build(2, all_hosts, IGMP_QUERY, none);
	igmp_packet_handler();
	build_tagged(4, 30, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PMASK_9 & ~(PORT_BIT(2) | PORT_BIT(4))), "router port outside the VLAN: report flooded in the VLAN");
	iso_block = PORT_BIT(3);
	build(8, all_hosts, IGMP_QUERY, none);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PMASK_9 & ~(PORT_BIT(3) | PORT_BIT(8))), "isolation: not sent where the ingress port may not go");
	iso_block = 0;

	igmp_enable();
	build(6, all_hosts, IGMP_QUERY, none);
	igmp_packet_handler();
	stp_enabled = true;
	memset(stp_state, 3, sizeof(stp_state));
	stp_state[6] = 1;
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PMASK_9 & ~(PORT_BIT(2) | PORT_BIT(6))) && lm_act == LOOKUP_MISS_FLOOD,
	      "querier port blocked by STP: forgotten, report flooded");
	stp_state[6] = 3;
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PMASK_9 & ~PORT_BIT(2)), "querier port forwarding again: not a querier port until a query");
	stp_enabled = false;

	igmp_enable();
	lag_members[2] = PORT_BIT(6) | PORT_BIT(7);
	build(6, all_hosts, IGMP_QUERY, none);
	igmp_packet_handler();
	link_up &= ~PORT_BIT(6);
	build(2, grp, IGMP_V2_REPORT, grp);
	CHECK(igmp_packet_handler() == 1 && ((struct wire_out *)&uip_buf[RTL_FRAME_DESC_SIZE])->rtl_tag.pmask
	      == HTONS(PORT_BIT(7)) && lm_act == LOOKUP_MISS_DROP_9, "querier behind a LAG: the other member keeps it");
	link_up = 0x3ff;
	lag_members[2] = 0;

	if (fails) {
		printf("test_igmp: %d FAILED\n", fails);
		return 1;
	}
	printf("test_igmp: ALL PASSED\n");
	return 0;
}

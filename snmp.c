/*
 * Minimal SNMPv1 / SNMPv2c agent for RTL837x switches.
 *
 * The agent is read-only and serves a curated subset of MIB-II
 * (see snmp.h). It runs on top of uIP as a UDP application on
 * port 161 and encodes/decodes BER by hand.
 *
 * Layout notes:
 *  - The whole module is banked into BANK2 like syslog/dhcp so
 *    the parser/encoder does not steal precious HOME code space.
 *  - The MIB is stored as a flat table of "descriptors" ordered
 *    lexicographically. GET-NEXT walks the table in order and
 *    within a table descriptor iterates over ifIndex.
 *  - Values are computed on the fly by a dispatch on the
 *    descriptor's handler id; nothing is cached.
 */

// #define DEBUG

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"
#include "rtl837x_port.h"
#include "machine.h"
#include "uip/uip.h"
#include "version.h"
#include "snmp.h"

#pragma codeseg BANK2
#pragma constseg BANK2
/*
 * Put every function in this translation unit on the software stack.
 * The 8051's internal RAM is already full without SNMP (see mem map);
 * pushing locals onto the software stack, which has ~130 free bytes
 * on this build, is the only way to fit the parser at all.
 */
#pragma stackauto

extern __code struct machine machine;
extern __xdata volatile uint32_t ticks;
extern __xdata struct uip_eth_addr uip_ethaddr;
extern __xdata uint8_t sfr_data[4];

/*
 * Globals for the module. Kept in xdata; the CPU has no cycles to
 * spare pushing large frames onto the software stack.
 */
__xdata struct snmp_state snmp_state;
__xdata uip_ipaddr_t snmp_server_ip;

/* Working buffer for the OID we are currently answering. Subids up to
 * 127 are all that the served MIB uses so uint8_t is enough. */
#define SNMP_OID_MAX 16
__xdata uint8_t snmp_oid[SNMP_OID_MAX];
__xdata uint8_t snmp_oid_len;

/* PDU state extracted from the request. request-id and error-status
 * are echoed back unchanged when the request itself is malformed;
 * for well-formed GetResponse we override error-status/error-index. */
__xdata uint8_t snmp_reqid[8];
__xdata uint8_t snmp_reqid_len;
__xdata uint8_t snmp_version;      /* 0 = v1, 1 = v2c */
__xdata uint8_t snmp_pdu_type;     /* 0xA0/0xA1/0xA5 in / 0xA2 out */
__xdata uint16_t snmp_community_off;
__xdata uint8_t snmp_community_len;

/* Sender address captured from the incoming datagram; the uIP UDP
 * connection is wildcard-listening so we have to plug the reply
 * destination in ourselves before uip_udp_send() runs. */
__xdata uip_ipaddr_t snmp_sender_ip;
__xdata uint16_t snmp_sender_port;

/*
 * ---- BER tag constants -------------------------------------------------
 */
#define BER_INTEGER	0x02
#define BER_OCTET_STR	0x04
#define BER_NULL	0x05
#define BER_OID		0x06
#define BER_SEQUENCE	0x30
#define BER_IPADDR	0x40
#define BER_COUNTER32	0x41
#define BER_GAUGE32	0x42
#define BER_TIMETICKS	0x43
#define BER_NOSUCHOBJ	0x80	/* SNMPv2 exception */
#define BER_NOSUCHINST	0x81
#define BER_ENDMIBVIEW	0x82

#define PDU_GET		0xA0
#define PDU_GETNEXT	0xA1
#define PDU_RESPONSE	0xA2
#define PDU_GETBULK	0xA5

/*
 * ---- MIB descriptor table ---------------------------------------------
 *
 * Each entry describes one MIB-II node past the fixed prefix
 * 1.3.6.1.2.1. `kind == 0` means a scalar (instance is .0),
 * `kind == 1` means an ifTable column (instance is .<ifIndex>).
 * The table is ordered lexicographically so GET-NEXT can just walk it.
 */
#define KIND_SCALAR	0
#define KIND_TABLE	1

/* Handler ids double as symbolic names for the switch in
 * snmp_encode_value(). */
enum {
	H_SYSDESCR = 1, H_SYSOBJID, H_SYSUPTIME, H_SYSCONTACT,
	H_SYSNAME, H_SYSLOCATION, H_SYSSERVICES,
	H_IFNUMBER,
	H_IFINDEX, H_IFDESCR, H_IFTYPE, H_IFMTU, H_IFSPEED,
	H_IFPHYS, H_IFADMIN, H_IFOPER,
	H_IFINOCTETS, H_IFINUCAST, H_IFINERR,
	H_IFOUTOCTETS, H_IFOUTUCAST, H_IFOUTERR,
};

struct mib_node {
	uint8_t suffix_len;
	uint8_t suffix[6];
	uint8_t kind;
	uint8_t handler;
};

/* MIB nodes past 1.3.6.1.2.1 . Kept in lex order. */
static __code const struct mib_node mib_table[] = {
	{ 2, { 1, 1 },       KIND_SCALAR, H_SYSDESCR },
	{ 2, { 1, 2 },       KIND_SCALAR, H_SYSOBJID },
	{ 2, { 1, 3 },       KIND_SCALAR, H_SYSUPTIME },
	{ 2, { 1, 4 },       KIND_SCALAR, H_SYSCONTACT },
	{ 2, { 1, 5 },       KIND_SCALAR, H_SYSNAME },
	{ 2, { 1, 6 },       KIND_SCALAR, H_SYSLOCATION },
	{ 2, { 1, 7 },       KIND_SCALAR, H_SYSSERVICES },
	{ 2, { 2, 1 },       KIND_SCALAR, H_IFNUMBER },
	{ 4, { 2, 2, 1, 1 }, KIND_TABLE,  H_IFINDEX },
	{ 4, { 2, 2, 1, 2 }, KIND_TABLE,  H_IFDESCR },
	{ 4, { 2, 2, 1, 3 }, KIND_TABLE,  H_IFTYPE },
	{ 4, { 2, 2, 1, 4 }, KIND_TABLE,  H_IFMTU },
	{ 4, { 2, 2, 1, 5 }, KIND_TABLE,  H_IFSPEED },
	{ 4, { 2, 2, 1, 6 }, KIND_TABLE,  H_IFPHYS },
	{ 4, { 2, 2, 1, 7 }, KIND_TABLE,  H_IFADMIN },
	{ 4, { 2, 2, 1, 8 }, KIND_TABLE,  H_IFOPER },
	{ 4, { 2, 2, 1, 10 }, KIND_TABLE, H_IFINOCTETS },
	{ 4, { 2, 2, 1, 11 }, KIND_TABLE, H_IFINUCAST },
	{ 4, { 2, 2, 1, 14 }, KIND_TABLE, H_IFINERR },
	{ 4, { 2, 2, 1, 16 }, KIND_TABLE, H_IFOUTOCTETS },
	{ 4, { 2, 2, 1, 17 }, KIND_TABLE, H_IFOUTUCAST },
	{ 4, { 2, 2, 1, 20 }, KIND_TABLE, H_IFOUTERR },
};
#define MIB_TABLE_LEN (sizeof(mib_table) / sizeof(mib_table[0]))

/* Fixed prefix subids for MIB-II: 1.3.6.1.2.1
 * (the 1.3 collapses to 0x2b in BER, handled separately). */
static __code const uint8_t mib_prefix[] = { 1, 3, 6, 1, 2, 1 };
#define MIB_PREFIX_LEN 6

/* sysObjectID.0 value for the playground. Under IANA experimental
 * (1.3.6.1.3) so no enterprise-number registration is required. */
static __code const uint8_t sys_obj_id[] = {
	1, 3, 6, 1, 3, 99, 99, 1
};
#define SYS_OBJ_ID_LEN (sizeof(sys_obj_id) / sizeof(sys_obj_id[0]))

/*
 * ---- Helpers -----------------------------------------------------------
 */

static uint8_t n_ports(void)
{
	return machine.max_port - machine.min_port + 1;
}

/* Convert 1-based ifIndex into a physical port label (as seen by user
 * on the case). Returns 0 for an out-of-range ifIndex. */
static uint8_t ifindex_phys(uint8_t idx)
{
	if (idx < 1 || idx > n_ports())
		return 0;
	return machine.log_to_phys_port[machine.min_port + idx - 1];
}

/* Compute the logical port number for a given 1-based ifIndex.
 * Caller must have validated idx via a prior ifindex_phys(). */
static uint8_t ifindex_log(uint8_t idx)
{
	return machine.min_port + idx - 1;
}

/*
 * ---- BER decoder helpers -----------------------------------------------
 */

/*
 * SDCC quirk: locals passed by reference to `__xdata *` parameters must
 * themselves live in __xdata; a bare local is placed in `near` memory
 * and a &local mismatch is a hard error. We keep those cross-address-
 * space scratch variables here at module scope.
 */
__xdata uint16_t ber_l;
__xdata uint32_t ber_iv;
__xdata uint8_t ber_inst;
__xdata uint8_t snmp_inst;

/* Parse a BER length starting at buf[*pos]. Advances *pos past the
 * length header and stores the value in *out. Returns 0 on error. */
static uint8_t ber_read_len(__xdata uint8_t *buf, __xdata uint16_t *pos,
			    uint16_t end, __xdata uint16_t *out)
{
	if (*pos >= end)
		return 0;
	uint8_t b = buf[(*pos)++];
	if (b < 0x80) {
		*out = b;
		return 1;
	}
	uint8_t nb = b & 0x7f;
	if (nb == 0 || nb > 2 || (*pos + nb) > end)
		return 0;
	uint16_t v = 0;
	while (nb--) {
		v = (v << 8) | buf[(*pos)++];
	}
	*out = v;
	return 1;
}

/* Read a signed BER integer up to 32 bits into *out.
 * Rejects lengths > 4 to avoid silent truncation. */
static uint8_t ber_read_int(__xdata uint8_t *buf, __xdata uint16_t *pos,
			    uint16_t end, __xdata uint32_t *out)
{
	if (*pos >= end || buf[*pos] != BER_INTEGER)
		return 0;
	(*pos)++;
	if (!ber_read_len(buf, pos, end, &ber_l))
		return 0;
	if (ber_l == 0 || ber_l > 4 || *pos + ber_l > end)
		return 0;
	uint32_t v = 0;
	while (ber_l--) {
		v = (v << 8) | buf[(*pos)++];
	}
	*out = v;
	return 1;
}

/* Decode a BER OID into snmp_oid[]. Advances *pos.
 * All subids in the served MIB fit in 7 bits so we reject anything
 * bigger to keep the on-wire comparison simple. */
static uint8_t ber_read_oid(__xdata uint8_t *buf, __xdata uint16_t *pos,
			    uint16_t end)
{
	if (*pos >= end || buf[*pos] != BER_OID)
		return 0;
	(*pos)++;
	if (!ber_read_len(buf, pos, end, &ber_l))
		return 0;
	if (ber_l == 0 || *pos + ber_l > end)
		return 0;
	uint16_t stop = *pos + ber_l;
	uint8_t n = 0;
	/* First byte encodes two subids: 40*a + b (a in 0..2, b < 40 when
	 * a < 2, otherwise unbounded). We only expect 0x2b (1.3) here. */
	uint8_t first = buf[(*pos)++];
	if (n + 2 > SNMP_OID_MAX)
		return 0;
	snmp_oid[n++] = first / 40;
	snmp_oid[n++] = first % 40;
	while (*pos < stop) {
		uint8_t b = buf[(*pos)++];
		/* Reject multi-byte varints - MIB-II subids fit in 7 bits. */
		if (b & 0x80)
			return 0;
		if (n >= SNMP_OID_MAX)
			return 0;
		snmp_oid[n++] = b;
	}
	snmp_oid_len = n;
	return 1;
}

/*
 * ---- OID lookup / next -------------------------------------------------
 */

/* Shared scratch for the walk/compare helpers - see snmp_process for
 * why these are module-scope in __xdata. */
static __xdata uint8_t lk_i, lk_k, lk_p, lk_np;
static __xdata int8_t lk_c;
static __xdata uint8_t oc_a_len, oc_i, oc_cand;

/* Compare snmp_oid[] against the prefix + a MIB node's suffix + a
 * trailing instance byte (either 0 or ifIndex). Returns:
 *   -1 if snmp_oid < candidate
 *    0 if equal
 *   +1 if snmp_oid > candidate
 */
static int8_t oid_cmp_candidate(__code const struct mib_node *n,
				uint8_t inst)
{
	oc_a_len = MIB_PREFIX_LEN + n->suffix_len + 1;
	for (oc_i = 0; oc_i < oc_a_len; oc_i++) {
		if (oc_i >= snmp_oid_len)
			return -1;
		if (oc_i < MIB_PREFIX_LEN)
			oc_cand = mib_prefix[oc_i];
		else if (oc_i < MIB_PREFIX_LEN + n->suffix_len)
			oc_cand = n->suffix[oc_i - MIB_PREFIX_LEN];
		else
			oc_cand = inst;
		if (snmp_oid[oc_i] < oc_cand)
			return -1;
		if (snmp_oid[oc_i] > oc_cand)
			return 1;
	}
	if (snmp_oid_len > oc_a_len)
		return 1;
	return 0;
}

/* Look up an exact GET target. On success returns the handler id and
 * fills *inst with the trailing ifIndex (or 0 for scalars). */
static uint8_t mib_lookup(__xdata uint8_t *inst)
{
	lk_np = n_ports();
	for (lk_i = 0; lk_i < MIB_TABLE_LEN; lk_i++) {
		if (mib_table[lk_i].kind == KIND_SCALAR) {
			if (oid_cmp_candidate(&mib_table[lk_i], 0) == 0) {
				*inst = 0;
				return mib_table[lk_i].handler;
			}
			continue;
		}
		/* Table column: iterate ifIndex 1..np. */
		lk_k = MIB_PREFIX_LEN + mib_table[lk_i].suffix_len;
		if (snmp_oid_len != lk_k + 1)
			continue;
		/* Fast prefix check to skip whole rows. */
		if (oid_cmp_candidate(&mib_table[lk_i], 1) > 0)
			continue;
		for (lk_p = 1; lk_p <= lk_np; lk_p++) {
			if (oid_cmp_candidate(&mib_table[lk_i], lk_p) == 0) {
				*inst = lk_p;
				return mib_table[lk_i].handler;
			}
		}
	}
	return 0;
}

/* Compute the OID immediately following snmp_oid[] in the served MIB.
 * On success updates snmp_oid[] to the found OID, sets *inst and
 * returns the handler id. Returns 0 when we walked off the end. */
static uint8_t mib_get_next(__xdata uint8_t *inst)
{
	lk_np = n_ports();
	for (lk_i = 0; lk_i < MIB_TABLE_LEN; lk_i++) {
		if (mib_table[lk_i].kind == KIND_SCALAR) {
			lk_c = oid_cmp_candidate(&mib_table[lk_i], 0);
			if (lk_c < 0) {
				for (lk_k = 0; lk_k < MIB_PREFIX_LEN; lk_k++)
					snmp_oid[lk_k] = mib_prefix[lk_k];
				for (lk_k = 0;
				     lk_k < mib_table[lk_i].suffix_len;
				     lk_k++)
					snmp_oid[MIB_PREFIX_LEN + lk_k] =
						mib_table[lk_i].suffix[lk_k];
				snmp_oid[MIB_PREFIX_LEN +
					 mib_table[lk_i].suffix_len] = 0;
				snmp_oid_len = MIB_PREFIX_LEN +
					mib_table[lk_i].suffix_len + 1;
				*inst = 0;
				return mib_table[lk_i].handler;
			}
			continue;
		}
		/* Table column: find first ifIndex whose OID is strictly
		 * greater than the target. */
		for (lk_p = 1; lk_p <= lk_np; lk_p++) {
			lk_c = oid_cmp_candidate(&mib_table[lk_i], lk_p);
			if (lk_c < 0) {
				for (lk_k = 0; lk_k < MIB_PREFIX_LEN; lk_k++)
					snmp_oid[lk_k] = mib_prefix[lk_k];
				for (lk_k = 0;
				     lk_k < mib_table[lk_i].suffix_len;
				     lk_k++)
					snmp_oid[MIB_PREFIX_LEN + lk_k] =
						mib_table[lk_i].suffix[lk_k];
				snmp_oid[MIB_PREFIX_LEN +
					 mib_table[lk_i].suffix_len] = lk_p;
				snmp_oid_len = MIB_PREFIX_LEN +
					mib_table[lk_i].suffix_len + 1;
				*inst = lk_p;
				return mib_table[lk_i].handler;
			}
		}
	}
	return 0;
}

/*
 * ---- Reading device state ---------------------------------------------
 */

/* Return current link speed for a logical port encoded as bits/s.
 * Speeds beyond 4 Gbit/s are capped to UINT32_MAX; ifHighSpeed is
 * not exposed. Returns 0 when link is down. */
static uint32_t port_link_speed(uint8_t log_port)
{
	uint8_t b;

	reg_read_m(RTL837X_REG_LINKS_STS);
	if (!((sfr_data[(log_port / 8) + 1] >> (log_port % 8)) & 1))
		return 0;

	if (log_port < 8)
		reg_read_m(RTL837X_REG_LINKS);
	else
		reg_read_m(RTL837X_REG_LINKS_89);
	b = sfr_data[3 - ((log_port & 7) >> 1)];
	b = (log_port & 1) ? b >> 4 : b & 0xf;

	switch (b) {
	case 0: return 10000000UL;
	case 1: return 100000000UL;
	case 2: return 1000000000UL;
	case 4: return 0xFFFFFFFFUL;	/* 10 Gbit/s > 32 bits */
	case 5: return 2500000000UL;
	case 6: return 0xFFFFFFFFUL;	/* 5 Gbit/s > 32 bits */
	default: return 0;
	}
}

static uint8_t port_link_up(uint8_t log_port)
{
	reg_read_m(RTL837X_REG_LINKS_STS);
	return (sfr_data[(log_port / 8) + 1] >> (log_port % 8)) & 1;
}

/* Read a 32-bit port statistic counter. */
static uint32_t port_counter(uint8_t log_port, uint8_t which)
{
	STAT_GET(which, log_port);
	reg_read_m(RTL837X_STAT_V_LOW);
	return ((uint32_t)sfr_data[0] << 24) |
	       ((uint32_t)sfr_data[1] << 16) |
	       ((uint32_t)sfr_data[2] << 8) |
	       sfr_data[3];
}

/*
 * ---- BER encoder helpers -----------------------------------------------
 *
 * We build the response by writing values into an in-memory buffer.
 * BER length fields are always encoded in their long form so that
 * the size of each header is known before its content: this lets us
 * fill headers in-place after we have written the content.
 */

/* Encode an unsigned 32-bit integer as a BER INTEGER content, i.e.
 * without the leading tag and length. Returns content length.
 * The top bit is padded with a 0x00 byte to keep values positive. */
static __xdata uint8_t pu_buf[5];
static uint8_t put_uint(__xdata uint8_t *p, uint32_t v)
{
	uint8_t n = 0;
	do {
		pu_buf[n++] = (uint8_t)v;
		v >>= 8;
	} while (v);
	if (pu_buf[n - 1] & 0x80)
		pu_buf[n++] = 0;
	uint8_t i;
	for (i = 0; i < n; i++)
		p[i] = pu_buf[n - 1 - i];
	return n;
}

/* Encode a BER OID content (without tag/length). Returns length. */
static uint8_t put_oid_bytes(__xdata uint8_t *p, __xdata const uint8_t *sub,
			     uint8_t n)
{
	if (n < 2)
		return 0;
	p[0] = sub[0] * 40 + sub[1];
	uint8_t i, o = 1;
	for (i = 2; i < n; i++)
		p[o++] = sub[i];
	return o;
}

/* Same as above but for a __code source array (used for sysObjectID). */
static uint8_t put_oid_bytes_c(__xdata uint8_t *p,
			       __code const uint8_t *sub, uint8_t n)
{
	if (n < 2)
		return 0;
	p[0] = sub[0] * 40 + sub[1];
	uint8_t i, o = 1;
	for (i = 2; i < n; i++)
		p[o++] = sub[i];
	return o;
}

/* Copy a __code string into the buffer, returning its length. */
static uint16_t put_cstr(__xdata uint8_t *p, __code const char *s)
{
	uint16_t n = 0;
	while (s[n]) {
		p[n] = s[n];
		n++;
	}
	return n;
}

/* Copy an __xdata string; returns its length. */
static uint16_t put_xstr(__xdata uint8_t *p, __xdata const char *s)
{
	uint16_t n = 0;
	while (s[n]) {
		p[n] = s[n];
		n++;
	}
	return n;
}

/*
 * ---- Value encoders ----------------------------------------------------
 *
 * Each encoder writes the full TLV (tag + length + value) for one MIB
 * leaf and returns the number of bytes written. `inst` is the ifIndex
 * for table columns and unused for scalars.
 *
 * Locals live at module scope in __xdata for the same overlay
 * exhaustion reason discussed above snmp_process().
 */
static __xdata uint16_t ev_vlen;
static __xdata uint8_t ev_tag;
static __xdata uint8_t * __xdata ev_v;
static __xdata uint32_t ev_u32;
static __xdata uint8_t ev_log, ev_phys, ev_i;

static uint16_t encode_value(__xdata uint8_t *p, uint8_t handler, uint8_t inst)
{
	ev_vlen = 0;
	ev_tag = BER_OCTET_STR;
	ev_v = p + 2;	/* leave 2 bytes for tag + short len */

	switch (handler) {
	case H_SYSDESCR:
		ev_vlen = put_cstr(ev_v, "RTLPlayground ");
		ev_vlen += put_cstr(ev_v + ev_vlen, VERSION_SW);
		ev_vlen += put_cstr(ev_v + ev_vlen, " on ");
		ev_vlen += put_cstr(ev_v + ev_vlen, machine.machine_name);
		break;
	case H_SYSOBJID:
		ev_tag = BER_OID;
		ev_vlen = put_oid_bytes_c(ev_v, sys_obj_id, SYS_OBJ_ID_LEN);
		break;
	case H_SYSUPTIME:
		ev_tag = BER_TIMETICKS;
		/* TimeTicks are hundredths of a second. `ticks` runs at
		 * SYS_TICK_HZ = 200 so divide by 2. */
		ev_vlen = put_uint(ev_v, ticks / (SYS_TICK_HZ / 100));
		break;
	case H_SYSCONTACT:
		ev_vlen = put_xstr(ev_v, snmp_state.contact);
		break;
	case H_SYSNAME:
		ev_vlen = put_xstr(ev_v, hostname);
		break;
	case H_SYSLOCATION:
		ev_vlen = put_xstr(ev_v, snmp_state.location);
		break;
	case H_SYSSERVICES:
		ev_tag = BER_INTEGER;
		ev_vlen = put_uint(ev_v, 3);	/* physical + datalink */
		break;
	case H_IFNUMBER:
		ev_tag = BER_INTEGER;
		ev_vlen = put_uint(ev_v, n_ports());
		break;
	case H_IFINDEX:
		ev_tag = BER_INTEGER;
		ev_vlen = put_uint(ev_v, inst);
		break;
	case H_IFDESCR:
		ev_phys = ifindex_phys(inst);
		ev_vlen = put_cstr(ev_v, "port ");
		if (ev_phys >= 10)
			ev_v[ev_vlen++] = '0' + (ev_phys / 10);
		ev_v[ev_vlen++] = '0' + (ev_phys % 10);
		break;
	case H_IFTYPE:
		ev_tag = BER_INTEGER;
		ev_vlen = put_uint(ev_v, 6);	/* ethernetCsmacd */
		break;
	case H_IFMTU:
		ev_tag = BER_INTEGER;
		ev_vlen = put_uint(ev_v, 1500);
		break;
	case H_IFSPEED:
		ev_tag = BER_GAUGE32;
		ev_log = ifindex_log(inst);
		ev_vlen = put_uint(ev_v, port_link_speed(ev_log));
		break;
	case H_IFPHYS:
		for (ev_i = 0; ev_i < 6; ev_i++)
			ev_v[ev_i] = uip_ethaddr.addr[ev_i];
		ev_vlen = 6;
		break;
	case H_IFADMIN:
		ev_tag = BER_INTEGER;
		ev_vlen = put_uint(ev_v, 1);	/* up */
		break;
	case H_IFOPER:
		ev_tag = BER_INTEGER;
		ev_log = ifindex_log(inst);
		ev_vlen = put_uint(ev_v, port_link_up(ev_log) ? 1 : 2);
		break;
	case H_IFINOCTETS:
	case H_IFOUTOCTETS:
		ev_tag = BER_COUNTER32;
		/* No direct byte counter available; report packet count
		 * for the moment. Tools tolerate the value as long as it
		 * only grows. */
		ev_log = ifindex_log(inst);
		ev_u32 = port_counter(ev_log, handler == H_IFINOCTETS ?
				      STAT_COUNTER_RX_PKTS :
				      STAT_COUNTER_TX_PKTS);
		ev_vlen = put_uint(ev_v, ev_u32);
		break;
	case H_IFINUCAST:
		ev_tag = BER_COUNTER32;
		ev_log = ifindex_log(inst);
		ev_vlen = put_uint(ev_v,
				   port_counter(ev_log, STAT_COUNTER_RX_PKTS));
		break;
	case H_IFOUTUCAST:
		ev_tag = BER_COUNTER32;
		ev_log = ifindex_log(inst);
		ev_vlen = put_uint(ev_v,
				   port_counter(ev_log, STAT_COUNTER_TX_PKTS));
		break;
	case H_IFINERR:
	case H_IFOUTERR:
		ev_tag = BER_COUNTER32;
		ev_log = ifindex_log(inst);
		ev_vlen = put_uint(ev_v,
				   port_counter(ev_log, STAT_COUNTER_ERR_PKTS));
		break;
	default:
		/* Should never happen; encode an empty octet string. */
		ev_vlen = 0;
		break;
	}

	/* Value is short (<128 bytes) so short-form length is always ok
	 * for our data set. Rewrite header at start. */
	p[0] = ev_tag;
	p[1] = (uint8_t)ev_vlen;
	return ev_vlen + 2;
}

/*
 * ---- Response builder --------------------------------------------------
 *
 * `req_end` is the byte one past the request PDU in uip_appdata.
 * We write the response into a temporary area past that end, then
 * copy it back to the start of uip_appdata before sending.
 *
 * The layout uses a fixed 3-byte length header (0x82 form) for the
 * outer SEQUENCE, the PDU tag, and the varbindlist so that their
 * sizes can be filled in at the end without shifting content.
 */

/* Write a single varbind (SEQ { OID, value }) into buf. Returns
 * number of bytes written. */
static __xdata uint8_t evb_oid_len;
static __xdata uint16_t evb_used;
static uint16_t emit_varbind(__xdata uint8_t *buf, uint8_t handler,
			     uint8_t inst)
{
	/* Reserve 4 bytes at the start for SEQ tag + 3-byte length so
	 * that the header size is known regardless of total length. */
	evb_oid_len = put_oid_bytes(buf + 4 + 2, snmp_oid, snmp_oid_len);
	buf[4 + 0] = BER_OID;
	buf[4 + 1] = evb_oid_len;
	evb_used = 2 + evb_oid_len;
	evb_used += encode_value(buf + 4 + evb_used, handler, inst);
	buf[0] = BER_SEQUENCE;
	buf[1] = 0x82;
	buf[2] = (uint8_t)(evb_used >> 8);
	buf[3] = (uint8_t)evb_used;
	return evb_used + 4;
}

/*
 * Process the whole PDU. `req_len` is the size of the incoming
 * datagram, returns the size of the response or 0 to drop.
 *
 * Nearly all scratch variables live in __xdata module scope on
 * purpose: the SDCC linker cannot always fit sizable per-function
 * frames into the 8051's internal RAM when many banked functions
 * are enabled at once. Since snmp_process is non-reentrant this
 * costs no correctness.
 */
static __xdata uint8_t * __xdata sp_buf;
static __xdata uint16_t sp_pos, sp_end, sp_l;
static __xdata uint32_t sp_iv;
static __xdata uint16_t sp_pdu_end, sp_vb_end, sp_vb_body_end;
static __xdata uint16_t sp_vbl;
static __xdata uint8_t * __xdata sp_out;
static __xdata uint16_t sp_out_pos;
static __xdata uint16_t sp_pdu_hdr_off, sp_err_status_off, sp_vbl_hdr_off;
static __xdata uint16_t sp_err_index, sp_vb_idx, sp_len;
static __xdata uint8_t sp_err_status, sp_handler, sp_pdu, sp_oid_content_len;
static __xdata uint16_t sp_used, sp_i;

static uint16_t snmp_process(uint16_t req_len)
{
	/* Anything larger than SCRATCH_OFF cannot be safely parsed while
	 * we simultaneously write the response into the same buffer. */
	if (req_len >= 720)
		return 0;
	sp_buf = uip_appdata;
	sp_pos = 0;
	sp_end = req_len;

	/* Outer SEQUENCE */
	if (sp_pos >= sp_end || sp_buf[sp_pos++] != BER_SEQUENCE)
		return 0;
	if (!ber_read_len(sp_buf, &sp_pos, sp_end, &sp_l))
		return 0;
	if (sp_pos + sp_l > sp_end)
		return 0;
	sp_end = sp_pos + sp_l;

	/* version */
	if (!ber_read_int(sp_buf, &sp_pos, sp_end, &sp_iv))
		return 0;
	if (sp_iv != 0 && sp_iv != 1)
		return 0;
	snmp_version = (uint8_t)sp_iv;

	/* community */
	if (sp_pos >= sp_end || sp_buf[sp_pos++] != BER_OCTET_STR)
		return 0;
	if (!ber_read_len(sp_buf, &sp_pos, sp_end, &sp_l))
		return 0;
	if (sp_l > SNMP_COMMUNITY_MAX || sp_pos + sp_l > sp_end)
		return 0;
	for (sp_i = 0; sp_i < sp_l; sp_i++) {
		if (sp_buf[sp_pos + sp_i] != snmp_state.community[sp_i])
			return 0;
	}
	if (snmp_state.community[sp_l] != 0)
		return 0;
	snmp_community_off = sp_pos;
	snmp_community_len = (uint8_t)sp_l;
	sp_pos += sp_l;

	/* PDU */
	if (sp_pos >= sp_end)
		return 0;
	sp_pdu = sp_buf[sp_pos++];
	if (sp_pdu != PDU_GET && sp_pdu != PDU_GETNEXT && sp_pdu != PDU_GETBULK)
		return 0;
	snmp_pdu_type = sp_pdu;
	if (!ber_read_len(sp_buf, &sp_pos, sp_end, &sp_l))
		return 0;
	if (sp_pos + sp_l > sp_end)
		return 0;
	sp_pdu_end = sp_pos + sp_l;

	/* request-id */
	if (sp_pos >= sp_pdu_end || sp_buf[sp_pos++] != BER_INTEGER)
		return 0;
	if (!ber_read_len(sp_buf, &sp_pos, sp_pdu_end, &sp_l))
		return 0;
	if (sp_l == 0 || sp_l > sizeof(snmp_reqid) || sp_pos + sp_l > sp_pdu_end)
		return 0;
	snmp_reqid_len = (uint8_t)sp_l;
	for (sp_i = 0; sp_i < sp_l; sp_i++)
		snmp_reqid[sp_i] = sp_buf[sp_pos + sp_i];
	sp_pos += sp_l;

	/* error-status / non-repeaters and error-index / max-repetitions.
	 * For our simple encoder we ignore max-repetitions and answer at
	 * most one variable-binding per request-varbind. */
	if (!ber_read_int(sp_buf, &sp_pos, sp_pdu_end, &sp_iv))
		return 0;
	if (!ber_read_int(sp_buf, &sp_pos, sp_pdu_end, &sp_iv))
		return 0;

	/* Varbind list */
	if (sp_pos >= sp_pdu_end || sp_buf[sp_pos++] != BER_SEQUENCE)
		return 0;
	if (!ber_read_len(sp_buf, &sp_pos, sp_pdu_end, &sp_l))
		return 0;
	if (sp_pos + sp_l > sp_pdu_end)
		return 0;
	sp_vb_end = sp_pos + sp_l;

	/*
	 * Build the response into a scratch area past the request. The uIP
	 * application data buffer is ~1502 bytes on this build; the split
	 * below keeps the response and the still-being-parsed request
	 * within that limit no matter which order the loop reads them:
	 *   [ request | gap | scratch response ]
	 *   0         req  SCRATCH_OFF         end
	 */
#define SCRATCH_OFF 720
#define SCRATCH_MAX 720
	sp_out = sp_buf + SCRATCH_OFF;
	sp_out_pos = 4;			/* reserve outer SEQ header */
	sp_out[sp_out_pos++] = BER_INTEGER;
	sp_out[sp_out_pos++] = 1;
	sp_out[sp_out_pos++] = snmp_version;
	sp_out[sp_out_pos++] = BER_OCTET_STR;
	sp_out[sp_out_pos++] = snmp_community_len;
	for (sp_i = 0; sp_i < snmp_community_len; sp_i++)
		sp_out[sp_out_pos++] = sp_buf[snmp_community_off + sp_i];
	sp_pdu_hdr_off = sp_out_pos;
	sp_out_pos += 4;			/* PDU tag + 0x82 + hi + lo */
	sp_out[sp_out_pos++] = BER_INTEGER;
	sp_out[sp_out_pos++] = snmp_reqid_len;
	for (sp_i = 0; sp_i < snmp_reqid_len; sp_i++)
		sp_out[sp_out_pos++] = snmp_reqid[sp_i];
	sp_err_status_off = sp_out_pos;
	sp_out[sp_out_pos++] = BER_INTEGER;
	sp_out[sp_out_pos++] = 1;
	sp_out[sp_out_pos++] = 0;		/* noError */
	sp_out[sp_out_pos++] = BER_INTEGER;
	sp_out[sp_out_pos++] = 1;
	sp_out[sp_out_pos++] = 0;		/* error-index */
	sp_vbl_hdr_off = sp_out_pos;
	sp_out_pos += 4;

	sp_err_index = 0;
	sp_err_status = 0;
	sp_vb_idx = 0;

	while (sp_pos < sp_vb_end) {
		sp_vb_idx++;
		/* Each varbind is SEQUENCE { OID, value }. */
		if (sp_buf[sp_pos++] != BER_SEQUENCE) {
			sp_err_status = 5;	/* genErr */
			sp_err_index = sp_vb_idx;
			break;
		}
		if (!ber_read_len(sp_buf, &sp_pos, sp_vb_end, &sp_vbl) ||
		    sp_pos + sp_vbl > sp_vb_end) {
			sp_err_status = 5;
			sp_err_index = sp_vb_idx;
			break;
		}
		sp_vb_body_end = sp_pos + sp_vbl;
		if (!ber_read_oid(sp_buf, &sp_pos, sp_vb_body_end)) {
			sp_err_status = 5;
			sp_err_index = sp_vb_idx;
			break;
		}
		/* Skip past whatever value follows to the end of the
		 * varbind (normally an ASN.1 NULL for GET/GETNEXT). */
		sp_pos = sp_vb_body_end;

		if (snmp_pdu_type == PDU_GET) {
			sp_handler = mib_lookup(&snmp_inst);
		} else {
			/* GETNEXT or GETBULK (treated identically here). */
			sp_handler = mib_get_next(&snmp_inst);
		}

		if (sp_handler == 0) {
			if (snmp_version == 0) {
				/* v1 has no exceptions; signal noSuchName. */
				sp_err_status = 2;
				sp_err_index = sp_vb_idx;
				break;
			}
			/* v2c: encode endOfMibView / noSuchObject */
			sp_oid_content_len =
				put_oid_bytes(sp_out + sp_out_pos + 4 + 2,
					      snmp_oid, snmp_oid_len);
			sp_out[sp_out_pos + 4 + 0] = BER_OID;
			sp_out[sp_out_pos + 4 + 1] = sp_oid_content_len;
			sp_used = 2 + sp_oid_content_len;
			sp_out[sp_out_pos + 4 + sp_used++] =
				(snmp_pdu_type == PDU_GET) ?
				BER_NOSUCHOBJ : BER_ENDMIBVIEW;
			sp_out[sp_out_pos + 4 + sp_used++] = 0;
			sp_out[sp_out_pos + 0] = BER_SEQUENCE;
			sp_out[sp_out_pos + 1] = 0x82;
			sp_out[sp_out_pos + 2] = (uint8_t)(sp_used >> 8);
			sp_out[sp_out_pos + 3] = (uint8_t)sp_used;
			sp_out_pos += sp_used + 4;
		} else {
			sp_out_pos += emit_varbind(sp_out + sp_out_pos,
						   sp_handler, snmp_inst);
		}

		/* Guard against a response overshooting the scratch area. */
		if (sp_out_pos > SCRATCH_MAX) {
			sp_err_status = 1;	/* tooBig */
			sp_err_index = 0;
			break;
		}
	}

	if (sp_err_status != 0) {
		/* Patch error-status / error-index and drop varbind list. */
		sp_out[sp_err_status_off + 2] = sp_err_status;
		sp_out[sp_err_status_off + 5] = (uint8_t)sp_err_index;
		sp_out_pos = sp_vbl_hdr_off + 4;
	}

	sp_len = sp_out_pos - (sp_vbl_hdr_off + 4);
	sp_out[sp_vbl_hdr_off] = BER_SEQUENCE;
	sp_out[sp_vbl_hdr_off + 1] = 0x82;
	sp_out[sp_vbl_hdr_off + 2] = (uint8_t)(sp_len >> 8);
	sp_out[sp_vbl_hdr_off + 3] = (uint8_t)sp_len;

	sp_len = sp_out_pos - (sp_pdu_hdr_off + 4);
	sp_out[sp_pdu_hdr_off] = PDU_RESPONSE;
	sp_out[sp_pdu_hdr_off + 1] = 0x82;
	sp_out[sp_pdu_hdr_off + 2] = (uint8_t)(sp_len >> 8);
	sp_out[sp_pdu_hdr_off + 3] = (uint8_t)sp_len;

	sp_len = sp_out_pos - 4;
	sp_out[0] = BER_SEQUENCE;
	sp_out[1] = 0x82;
	sp_out[2] = (uint8_t)(sp_len >> 8);
	sp_out[3] = (uint8_t)sp_len;

	/* Copy the response back to the start of uip_appdata. */
	for (sp_i = 0; sp_i < sp_out_pos; sp_i++)
		sp_buf[sp_i] = sp_out[sp_i];
	return sp_out_pos;
}

/*
 * ---- Public API --------------------------------------------------------
 */

void snmp_init(void) __banked __reentrant
{
	snmp_state.enabled = 0;
	snmp_state.conn = 0;
	/* Default community. */
	snmp_state.community[0] = 'p';
	snmp_state.community[1] = 'u';
	snmp_state.community[2] = 'b';
	snmp_state.community[3] = 'l';
	snmp_state.community[4] = 'i';
	snmp_state.community[5] = 'c';
	snmp_state.community[6] = 0;
	snmp_state.location[0] = 0;
	snmp_state.contact[0] = 0;
}

void snmp_start(void) __banked __reentrant
{
	if (snmp_state.conn != 0) {
		print_string_no_syslog("SNMP already running\n");
		return;
	}
	/* Listen on port 161, wildcard remote so any manager can reach us. */
	snmp_server_ip[0] = 0;
	snmp_server_ip[1] = 0;
	snmp_state.conn = uip_udp_new(&snmp_server_ip, 0);
	if (!snmp_state.conn) {
		print_string("SNMP: failed to create UDP socket\n");
		return;
	}
	uip_udp_bind(snmp_state.conn, HTONS(SNMP_PORT));
	snmp_state.enabled = 1;
	print_string("SNMP agent started on port 161\n");
}

void snmp_stop(void) __banked __reentrant
{
	snmp_state.enabled = 0;
	if (snmp_state.conn != 0) {
		uip_udp_remove(snmp_state.conn);
		snmp_state.conn = 0;
		print_string("SNMP agent stopped\n");
	}
}

/* Called from udp_callbacks() for every UDP event. */
void snmp_callback(uint16_t lport) __banked __reentrant
{
	if (lport != HTONS(SNMP_PORT))
		return;
	if (!snmp_state.enabled || !snmp_state.conn)
		return;

	if (uip_newdata()) {
		/* Capture the sender before we possibly reply. */
		__xdata struct uip_udpip_hdr *hdr =
			(__xdata struct uip_udpip_hdr *)
			&uip_buf[UIP_LLH_LEN];
		snmp_sender_ip[0] = hdr->srcipaddr[0];
		snmp_sender_ip[1] = hdr->srcipaddr[1];
		snmp_sender_port = hdr->srcport;

		uint16_t rlen = snmp_process(uip_datalen());
		if (rlen == 0) {
			uip_len = 0;
			return;
		}
		/* Direct this response at the manager. The conn is
		 * cleaned up on the next periodic poll below. */
		snmp_state.conn->ripaddr[0] = snmp_sender_ip[0];
		snmp_state.conn->ripaddr[1] = snmp_sender_ip[1];
		snmp_state.conn->rport = snmp_sender_port;
		uip_udp_send(rlen);
		return;
	}
	/* Periodic poll: reset remote address so the socket accepts
	 * requests from any manager on subsequent packets. */
	snmp_state.conn->ripaddr[0] = 0;
	snmp_state.conn->ripaddr[1] = 0;
	snmp_state.conn->rport = 0;
	uip_len = 0;
}

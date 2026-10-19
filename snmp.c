/* Bounded read-only SNMPv2c subset. No allocation, recursion or logging. */
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#ifdef RTLP_HOST_TEST
extern bool EA;
#else
#include <8051.h>
#endif
#include "uip/uip.h"
#include "machine.h"
#include "version.h"
#include "snmp.h"
#include "snmp_internal.h"
#include "snmp_hw.h"
#pragma codeseg BANK3
#pragma constseg BANK3
extern __code const struct machine machine;
__xdata uint8_t snmp_response[SNMP_MAX_MESSAGE];
volatile __xdata uint32_t snmp_uptime;
__xdata uint8_t snmp_community[32];
__xdata uint8_t snmp_community_length, snmp_network[4], snmp_mask[4];
__xdata uint8_t snmp_agent_enabled;
__xdata struct uip_udp_conn *__xdata snmp_listener;
struct binding {
	uint16_t offset, length, value_offset, value_length;
};
static __xdata struct binding bindings[SNMP_MAX_VARBINDS];
static __xdata uint32_t arcs[SNMP_MAX_ARCS];
static __xdata uint8_t arc_count, candidate[12], candidate_len;
static __xdata struct snmp_counter counter;
static __xdata uint16_t speed;
static __xdata const uint8_t *__xdata request;
static __xdata uint16_t cursor, pos;
static __xdata uint8_t overflow, count, pdu, request_id[4], id_length;
/* GETBULK fields saturate to 0..0xffff; ordinals number the MIB's slots, 0 = not started. */
static __xdata uint16_t field, bulk_nonrep, bulk_max, slot_no, rep_slot[SNMP_MAX_VARBINDS];
static __xdata uint8_t excess;
#define INT_SKIP 0
#define INT_ID 1
#define INT_FIELD 2
#define REP_ENDED 0xffff
/* column lists are in numeric order, independently of selector order. */
static __code const uint8_t syscols[] = {1, 3, 4, 5, 6};
static __code const uint8_t ifcols[] = {1, 2, 3, 5, 7, 8, 10, 11, 14, 16, 17, 20};
static __code const uint8_t xcols[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 18};

void snmp_init(void) __banked
{
	snmp_hw_init();
	memset(snmp_network, 0, 4);
	memset(snmp_mask, 0, 4);
	{
		bool enabled = EA;
		EA = 0;
		snmp_uptime = 0;
		EA = enabled;
	}
	snmp_agent_enabled = 0;
	snmp_community_length = 0;
	snmp_listener = 0;
}

/* TLV bounds always refer to the immediate parent, not the whole packet. */
static bool tlv(uint8_t tag, __xdata uint16_t end, volatile __xdata uint16_t *__xdata child_end)
{
	__xdata uint16_t length;
	__xdata uint8_t n;
	if (cursor >= end || request[cursor++] != tag || cursor >= end)
		return false;
	n = request[cursor++];
	length = n;
	if (n & 0x80) {
		n &= 0x7f;
		if (!n || n > 2 || (uint16_t)(end - cursor) < n)
			return false;
		length = 0;
		while (n--)
			length = (length << 8) | request[cursor++];
	}
	if (length > end - cursor)
		return false;
	*child_end = cursor + length;
	return true;
}

static bool integer(uint16_t end, __xdata uint8_t mode)
{
	/* SDCC #15242 otherwise spills these XDATA temporaries into scarce DATA. */
	__xdata volatile uint16_t e, n, o;
	__xdata volatile uint8_t i;
	if (!tlv(ASN_INTEGER, end, &e))
		return false;
	n = e - cursor;
	if (!n || n > 4)
		return false;
	if (n > 1 && ((request[cursor] == 0 && !(request[cursor + 1] & 0x80)) ||
		      (request[cursor] == 255 && (request[cursor + 1] & 0x80))))
		return false;
	if (mode == INT_ID) {
		id_length = n;
		for (i = 0; i < n; ++i)
			request_id[i] = request[cursor + i];
	} else if (mode == INT_FIELD) {
		/* Negative is 0. A minimal positive value needing more than 16 bits saturates. */
		field = 0;
		o = cursor;
		if (!(request[o] & 0x80)) {
			if (n == 3 && !request[o]) {
				++o;
				--n;
			}
			if (n > 2)
				field = 0xffff;
			else
				while (n--)
					field = (field << 8) | request[o++];
		}
	}
	cursor = e;
	return true;
}

static bool oid(uint16_t offset, __xdata uint16_t length)
{
	__xdata uint16_t e = offset + length;
	__xdata uint32_t value = 0;
	__xdata uint8_t b, first = 1, start = 1;
	arc_count = 0;
	if (!length)
		return false;
	while (offset < e) {
		b = request[offset++];
		if (start && b == 0x80)
			return false;
		if (value > 0x01ffffffUL) {
			/* Combined first subidentifier can be UINT32_MAX + 80. */
			if (!first || value != 0x02000000UL || (b & 0x80) || b > 79)
				return false;
			arcs[arc_count++] = 2;
			arcs[arc_count++] = ((value << 7) | b) - 80;
			first = 0;
			value = 0;
			start = 1;
			continue;
		}
		value = (value << 7) | (b & 0x7f);
		start = 0;
		if (!(b & 0x80)) {
			if (first) {
				arcs[arc_count++] = value < 40 ? 0 : value < 80 ? 1 : 2;
				value -= value < 40 ? 0 : value < 80 ? 40 : 80;
				first = 0;
			}
			if (arc_count == SNMP_MAX_ARCS)
				return false;
			arcs[arc_count++] = value;
			value = 0;
			start = 1;
		}
	}
	return start && !first;
}

static bool parse(uint16_t length)
{
	__xdata volatile uint16_t outer, comm, pend, list, bind, name, value, boff, voff;
	__xdata volatile uint8_t mode;
	cursor = 0;
	count = 0;
	excess = 0;
	if (!tlv(ASN_SEQUENCE, length, &outer) || outer != length)
		return false;
	if (!tlv(ASN_INTEGER, outer, &value) || value - cursor != 1 || request[cursor] != SNMP_VERSION_2C)
		return false;
	cursor = value;
	if (!tlv(ASN_OCTET_STRING, outer, &comm) || comm - cursor != snmp_community_length ||
	    memcmp(request + cursor, snmp_community, snmp_community_length))
		return false;
	cursor = comm;
	if (cursor >= outer)
		return false;
	pdu = request[cursor];
	if (pdu != SNMP_PDU_GET && pdu != SNMP_PDU_GETNEXT && pdu != SNMP_PDU_GETBULK)
		return false;
	if (!tlv(pdu, outer, &pend) || pend != outer || !integer(pend, INT_ID))
		return false;
	/* Two INTEGERs: error fields (ignored, RFC3416) or GETBULK non-repeaters/max-repetitions. */
	mode = pdu == SNMP_PDU_GETBULK ? INT_FIELD : INT_SKIP;
	if (!integer(pend, mode))
		return false;
	bulk_nonrep = field;
	if (!integer(pend, mode) || !tlv(ASN_SEQUENCE, pend, &list) || list != pend)
		return false;
	bulk_max = field;
	/* Every binding is validated; only the first SNMP_MAX_VARBINDS are kept, the rest flag tooBig. */
	while (cursor < list) {
		if (!tlv(ASN_SEQUENCE, list, &bind) || !tlv(ASN_OBJECT_ID, bind, &name))
			return false;
		boff = cursor;
		if (!oid(cursor, name - cursor))
			return false;
		cursor = name;
		voff = cursor;
		if (cursor >= bind || !tlv(request[cursor], bind, &value) || value != bind)
			return false;
		{
			__xdata uint8_t tag;
			__xdata uint16_t n;
			tag = request[voff];
			n = value - cursor;
			if (tag == ASN_NULL || tag == SNMP_NO_SUCH_OBJECT ||
			    tag == SNMP_NO_SUCH_INSTANCE || tag == SNMP_END_OF_MIB) {
				if (n)
					return false;
			} else if (tag == ASN_INTEGER || tag == ASN_COUNTER32 ||
				   tag == ASN_GAUGE32 || tag == ASN_TIMETICKS ||
				   tag == ASN_COUNTER64 || tag == ASN_UNSIGNED32) {
				if (!n || n > (tag == ASN_INTEGER     ? 4
					       : tag == ASN_COUNTER64 ? 9
								      : 5))
					return false;
				if (n > 1 &&
				    ((request[cursor] == 0 && !(request[cursor + 1] & 0x80)) ||
				     (request[cursor] == 255 && (request[cursor + 1] & 0x80))))
					return false;
				if (tag != ASN_INTEGER &&
				    ((request[cursor] & 0x80) ||
				     (n == (tag == ASN_COUNTER64 ? 9 : 5) && request[cursor])))
					return false;
			} else if (tag == ASN_OBJECT_ID) {
				if (!oid(cursor, n))
					return false;
			} else if (tag == ASN_IP_ADDRESS) {
				if (n != 4)
					return false;
			} else if (tag != ASN_OCTET_STRING && tag != ASN_OPAQUE)
				return false;
		}
		if (count == SNMP_MAX_VARBINDS)
			excess = 1;
		else {
			bindings[count].offset = boff;
			bindings[count].length = name - boff;
			bindings[count].value_offset = voff;
			bindings[count].value_length = bind - voff;
			++count;
		}
		cursor = bind;
	}
	return cursor == list;
}

static uint8_t chip_port(uint8_t index)
{
	__xdata uint8_t i;
	for (i = machine.min_port; i <= machine.max_port && i < CPU_PORT; ++i)
		if (machine.log_to_phys_port[i] == index)
			return i;
	return 255;
}

static void make_oid(uint8_t group, __xdata uint8_t col, __xdata uint8_t index)
{
	candidate[0] = 1;
	candidate[1] = 3;
	candidate[2] = 6;
	candidate[3] = 1;
	candidate[4] = 2;
	candidate[5] = 1;
	candidate[6] = group;
	if (group == 1) {
		candidate[7] = col;
		candidate[8] = 0;
		candidate_len = 9;
	} else if (!col) {
		candidate[7] = 1;
		candidate[8] = 0;
		candidate_len = 9;
	} else {
		candidate[7] = group == 2 ? 2 : 1;
		candidate[8] = 1;
		if (group == 31) {
			candidate[9] = 1;
			candidate[10] = col;
			candidate[11] = index;
			candidate_len = 12;
		} else {
			candidate[9] = col;
			candidate[10] = index;
			candidate_len = 11;
		}
	}
}
static int8_t compare(void)
{
	__xdata uint8_t i, n = arc_count < candidate_len ? arc_count : candidate_len;
	for (i = 0; i < n; ++i) {
		if (arcs[i] < candidate[i])
			return -1;
		if (arcs[i] > candidate[i])
			return 1;
	}
	return arc_count < candidate_len ? -1 : arc_count > candidate_len ? 1 : 0;
}
/* Supported object prefixes distinguish noSuchInstance from noSuchObject. */
static bool object_prefix(void)
{
	__xdata uint8_t i;
	if (arc_count < candidate_len - 1)
		return false;
	for (i = 0; i < candidate_len - 1; ++i)
		if (arcs[i] != candidate[i])
			return false;
	return true;
}
static uint8_t lookup(__xdata uint16_t resume)
{
	/* SDCC 4.5.0 #15242 otherwise emits three one-byte DATA spills
	 * (_lookup_sloc0/1/4), reducing the already small stack region.
	 * Single main-loop owner: volatile controls placement, not concurrency. */
	__xdata volatile uint8_t group, column, index, c, prefix = 0;
	/* resume 0: first object after (GET: equal to) the OID in arcs; else the first object
	 * after ordinal `resume`. Ordinals count every slot, absent ports included. */
	__xdata volatile uint16_t s = 0;
	for (group = 1; group <= 31; group = group == 1 ? 2 : group == 2 ? 31 : 32) {
		for (c = 0; c < (group == 1   ? sizeof(syscols)
				 : group == 2 ? sizeof(ifcols) + 1
					      : sizeof(xcols));
		     ++c) {
			column = group == 1   ? syscols[c]
				 : group == 2 ? (c ? ifcols[c - 1] : 0)
					      : xcols[c];
			for (index = 1; index <= (group == 1 || !column ? 1 : 9); ++index) {
				++s;
				if (s < resume)
					continue;
				if (group != 1 && column && chip_port(index) == 255)
					continue;
				make_oid(group, column, index);
				if (object_prefix())
					prefix = 1;
				if (resume ? s > resume
				    : pdu != SNMP_PDU_GET ? compare() < 0 : compare() == 0) {
					slot_no = s;
					if ((group == 2 && column == 5) ||
					    (group == 31 && column == 15)) {
						if (!snmp_hw_speed(chip_port(index), &speed))
							return SNMP_LOOKUP_HW_ERROR;
					}
					return 0;
				}
			}
		}
	}
	return pdu != SNMP_PDU_GET ? SNMP_END_OF_MIB
	       : prefix		       ? SNMP_NO_SUCH_INSTANCE
				       : SNMP_NO_SUCH_OBJECT;
}

/* Reverse writer: payload precedes its header, with checked subtraction. */
static void put(uint8_t b)
{
	if (!pos) {
		overflow = 1;
		return;
	}
	snmp_response[--pos] = b;
}
static void wrap(uint8_t tag, __xdata uint16_t end)
{
	__xdata uint16_t length = end - pos;
	if (length < 128)
		put(length);
	else {
		put(length);
		if (length > 255) {
			put(length >> 8);
			put(0x82);
		} else
			put(0x81);
	}
	put(tag);
}
static void bytes(uint8_t tag, __xdata const uint8_t *__xdata data, __xdata uint16_t length)
{
	__xdata uint16_t end = pos;
	while (length)
		put(data[--length]);
	wrap(tag, end);
}
static void number(uint8_t tag, __xdata uint32_t value)
{
	__xdata uint16_t end = pos;
	__xdata uint8_t last;
	do {
		last = value;
		put(last);
		value >>= 8;
	} while (value);
	if (last & 0x80)
		put(0);
	wrap(tag, end);
}
static void text_x(__xdata const char *s, __xdata uint8_t cap)
{
	__xdata uint8_t n = 0;
	while (n < cap && s[n])
		++n;
	bytes(ASN_OCTET_STRING, (const __xdata uint8_t *)s, n);
}
static void native_counter(uint8_t tag)
{
	__xdata uint8_t start = tag == ASN_COUNTER64 ? 0 : 4, i = 8;
	__xdata uint16_t end = pos;
	while (start < 7 && !counter.bytes[start])
		++start;
	while (i > start)
		put(counter.bytes[--i]);
	if (counter.bytes[start] & 0x80)
		put(0);
	wrap(tag, end);
}
static void candidate_oid(void)
{
	__xdata uint8_t i = candidate_len;
	__xdata uint16_t end = pos;
	while (i > 2)
		put(candidate[--i]);
	put(43);
	wrap(ASN_OBJECT_ID, end);
}
static bool value(void)
{
	__xdata volatile uint8_t group = candidate[6], col = candidate[7], index, port, selector;
	__xdata uint32_t v;
	__xdata bool enabled;
	if (group == 1) {
		if (col == 1) {
			__xdata uint16_t end, n;
			end = pos;
			n = sizeof(VERSION_SW) - 1;
			while (n)
				put(VERSION_SW[--n]);
			n = sizeof(" RTLPlayground ") - 1;
			while (n)
				put(" RTLPlayground "[--n]);
			n = 0;
			while (n < sizeof(machine.machine_name) && machine.machine_name[n])
				++n;
			while (n)
				put(machine.machine_name[--n]);
			wrap(ASN_OCTET_STRING, end);
		} else if (col == 5)
			text_x(hostname, sizeof(hostname));
		else if (col == 4 || col == 6)
			bytes(ASN_OCTET_STRING, snmp_community, 0);
		else {
			enabled = EA;
			EA = 0;
			v = snmp_uptime;
			EA = enabled;
			number(ASN_TIMETICKS, v);
		}
		return true;
	}
	if (candidate_len == 9) {
		v = 0;
		for (index = 1; index <= 9; ++index)
			if (chip_port(index) != 255)
				++v;
		number(ASN_INTEGER, v);
		return true;
	}
	col = candidate[candidate_len - 2];
	index = candidate[candidate_len - 1];
	port = chip_port(index);
	if (group == 31 && col == 18) {
		text_x(port_names[port], PORT_NAME_SIZE);
		return true;
	}
	if (group == 2 && col == 7) {
		__xdata uint8_t state;
		if (!snmp_hw_admin(port, &state))
			return false;
		number(ASN_INTEGER, state);
		return true;
	}
	if (group == 2 && (col == 14 || col == 20)) {
		if (!snmp_hw_counter32(port, 48, col == 14, &counter))
			return false;
		native_counter(ASN_COUNTER32);
		return true;
	}
	if (group == 2 && col == 1) {
		number(ASN_INTEGER, index);
		return true;
	}
	if ((group == 2 && col == 2) || (group == 31 && col == 1)) {
		/* Stable front-panel identity, including SFP; no CPU row. */
		uint16_t end = pos;
		put('0' + index);
		put(' ');
		if (machine.is_sfp[port]) {
			put('P');
			put('F');
			put('S');
		} else {
			put('t');
			put('r');
			put('o');
			put('P');
		}
		wrap(ASN_OCTET_STRING, end);
		return true;
	}
	if (group == 2 && col == 3) {
		number(ASN_INTEGER, 6);
		return true;
	}
	if ((group == 2 && col == 5) || (group == 31 && col == 15)) {
		if (group == 31)
			v = speed;
		else {
			switch (speed) {
			case 0:
				v = 0;
				break;
			case 10:
				v = 10000000UL;
				break;
			case 100:
				v = 100000000UL;
				break;
			case 1000:
				v = 1000000000UL;
				break;
			case 2500:
				v = 2500000000UL;
				break;
			default:
				v = 0xffffffffUL;
				break;
			}
		}
		number(ASN_GAUGE32, v);
		return true;
	}
	if (group == 2 && col == 8) {
		if (!snmp_hw_link(port, &v))
			return false;
		/* chip_port() maps only MAC ports 0..8; a 16-bit mask is sufficient. */
		number(ASN_INTEGER, (v & ((uint16_t)1 << port)) ? 1 : 2);
		return true;
	}
	if (group == 31 && col >= 2 && col <= 5)
		selector = col < 4 ? col + 1 : col + 2;
	else
		selector = group == 31 ? (col == 6    ? 0
					  : col == 10 ? 1
					  : col < 10  ? col - 5
						      : col - 6)
			   : col == 10 ? 0
			   : col == 11 ? 2
			   : col == 16 ? 1
				       : 5;
	if (!snmp_hw_counter(port, selector, &counter))
		return false;
	native_counter(group == 31 && col >= 6 ? ASN_COUNTER64 : ASN_COUNTER32);
	return true;
}
/* Exact size of the whole response message for a varbind list of `list` bytes (see envelope). */
static uint16_t hdr_len(__xdata uint16_t n)
{
	return n < 128 ? 2 : n < 256 ? 3 : 4;
}
static uint16_t message_size(__xdata uint16_t list)
{
	__xdata uint16_t body;
	/* PDU: list, request-id, error-status and error-index (3 bytes each). */
	body = hdr_len(list) + list + 2 + id_length + 3 + 3;
	/* Message: version, community, PDU. */
	body += 3 + 2 + snmp_community_length + hdr_len(body);
	return hdr_len(body) + body;
}
static void reverse(__xdata uint16_t lo, __xdata uint16_t hi)
{
	__xdata uint8_t t;
	while (lo + 1 < hi) {
		--hi;
		t = snmp_response[lo];
		snmp_response[lo++] = snmp_response[hi];
		snmp_response[hi] = t;
	}
}
/* GETBULK (RFC3416 4.2.3). Varbinds are produced in wire order. The reverse writer stacks each
 * one below the previous, so each block is byte-reversed as it completes and the whole list
 * once more at the end: block order and contents are then correct without a copy. Per-repeater
 * state is one MIB ordinal. Output stops at the first varbind that no longer fits in
 * SNMP_MAX_MESSAGE (a prefix, RFC3416 4.2.3), after max-repetitions rows, or after a row of
 * endOfMibView only: rows are limited by the response size, never by the request fields.
 * Returns the 1-based input binding that failed in hardware, else 0. */
static uint8_t bulk(void)
{
	__xdata uint8_t b = 0, nrep, live = 0, hit;
	__xdata uint16_t row = 0, end, st;
	nrep = bulk_nonrep < count ? bulk_nonrep : count;
	memset(rep_slot, 0, sizeof(rep_slot));
	for (;;) {
		if (b == nrep && !bulk_max)
			break;
		if (b == count) {
			if (!live || ++row >= bulk_max)
				break;
			b = nrep;
			live = 0;
		}
		st = rep_slot[b];
		end = pos;
		if (st == REP_ENDED)
			hit = SNMP_END_OF_MIB;
		else {
			if (!st)
				oid(bindings[b].offset, bindings[b].length);
			hit = lookup(st);
		}
		if (hit == SNMP_LOOKUP_HW_ERROR)
			return b + 1;
		if (hit) {
			put(0);
			put(hit);
			/* A walked-off repeater repeats its last object (still in candidate). */
			if (st && st != REP_ENDED)
				candidate_oid();
			else {
				bytes(ASN_OBJECT_ID, request + bindings[b].offset, bindings[b].length);
				rep_slot[b] = REP_ENDED;
			}
		} else {
			if (!value())
				return b + 1;
			candidate_oid();
			rep_slot[b] = slot_no;
			if (b >= nrep)
				live = 1;
		}
		wrap(ASN_SEQUENCE, end);
		if (overflow || message_size(SNMP_MAX_MESSAGE - pos) > SNMP_MAX_MESSAGE) {
			pos = end;
			overflow = 0;
			break;
		}
		reverse(pos, end);
		++b;
	}
	reverse(pos, SNMP_MAX_MESSAGE);
	return 0;
}
static void envelope(uint16_t list_end, __xdata uint8_t error, __xdata uint8_t error_index)
{
	__xdata uint16_t pend, end;
	wrap(ASN_SEQUENCE, list_end);
	pend = SNMP_MAX_MESSAGE;
	number(ASN_INTEGER, error_index);
	number(ASN_INTEGER, error);
	bytes(ASN_INTEGER, request_id, id_length);
	wrap(SNMP_PDU_RESPONSE, pend);
	bytes(ASN_OCTET_STRING, snmp_community, snmp_community_length);
	number(ASN_INTEGER, 1);
	end = SNMP_MAX_MESSAGE;
	wrap(ASN_SEQUENCE, end);
}
uint16_t snmp_process(__xdata const uint8_t *__xdata input, __xdata uint16_t length,
		      __xdata const uint8_t *__xdata source) __banked
{
	__xdata volatile uint8_t i, exception, error = 0, error_index = 0;
	__xdata volatile uint16_t end;
	if (!snmp_agent_enabled || !snmp_community_length || !input || !source ||
	    length > SNMP_MAX_MESSAGE)
		return 0;
	if (!source[0] || source[0] == 127 || source[0] >= 224)
		return 0;
	for (i = 0; i < 4; ++i)
		if ((source[i] & snmp_mask[i]) != snmp_network[i])
			return 0;
	if (snmp_mask[0] != 255 || snmp_mask[1] != 255 || snmp_mask[2] != 255 ||
	    snmp_mask[3] < 254) {
		__xdata uint8_t host_set, host_clear;
		host_set = host_clear = 0;
		for (i = 0; i < 4; ++i) {
			host_set |= source[i] & (uint8_t)~snmp_mask[i];
			host_clear |= (uint8_t)~source[i] & (uint8_t)~snmp_mask[i];
		}
		if (!host_set || !host_clear)
			return 0;
	}
	request = input;
	if (!parse(length))
		return 0;
	pos = SNMP_MAX_MESSAGE;
	overflow = 0;
	if (excess)
		error = SNMP_ERR_TOO_BIG; /* valid but too many bindings to store: empty list */
	else if (pdu == SNMP_PDU_GETBULK) {
		error_index = bulk();
		if (error_index)
			error = SNMP_ERR_GEN_ERR;
	} else {
		i = count;
		while (i) {
			--i;
			end = pos;
			oid(bindings[i].offset, bindings[i].length);
			exception = lookup(0);
			if (exception == SNMP_LOOKUP_HW_ERROR) {
				error = SNMP_ERR_GEN_ERR;
				error_index = i + 1;
				break;
			}
			if (exception) {
				put(0);
				put(exception);
				bytes(ASN_OBJECT_ID, request + bindings[i].offset, bindings[i].length);
			} else {
				if (!value()) {
					error = SNMP_ERR_GEN_ERR;
					error_index = i + 1;
					break;
				}
				candidate_oid();
			}
			wrap(ASN_SEQUENCE, end);
		}
	}
	if (error == SNMP_ERR_GEN_ERR) {
		/* genErr echoes the entire original list, with correct error-index. */
		pos = SNMP_MAX_MESSAGE;
		overflow = 0;
		i = count;
		while (i) {
			--i;
			end = pos;
			{
				__xdata uint16_t n;
				n = bindings[i].value_length;
				while (n)
					put(request[bindings[i].value_offset + --n]);
			}
			bytes(ASN_OBJECT_ID, request + bindings[i].offset, bindings[i].length);
			wrap(ASN_SEQUENCE, end);
		}
	}
	envelope(SNMP_MAX_MESSAGE, error, error_index);
	if (overflow) {
		pos = SNMP_MAX_MESSAGE;
		overflow = 0;
		envelope(SNMP_MAX_MESSAGE, SNMP_ERR_TOO_BIG, 0); /* tooBig, empty bindings */
	}
	if (overflow)
		return 0;
	length = SNMP_MAX_MESSAGE - pos;
	for (cursor = 0; cursor < length; ++cursor)
		snmp_response[cursor] = snmp_response[pos + cursor];
	return length;
}
void snmp_callback(void) __banked
{
	__xdata struct uip_udpip_hdr *header;
	__xdata uint16_t length;
	if (!snmp_listener || uip_udp_conn != snmp_listener || !uip_newdata())
		return;
	header = (__xdata struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN];
	if (!uip_ipaddr_cmpx(header->destipaddr, uip_hostaddr))
		return;
	if (!header->srcport)
		return;
	length = snmp_process(uip_appdata, uip_datalen(), (const __xdata uint8_t *)header->srcipaddr);
	if (length) {
		memcpy(uip_appdata, snmp_response, length);
		uip_udp_send(length);
	}
}

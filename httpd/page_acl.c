#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"
#include "rtl837x_acl.h"
#include "page_impl.h"

#pragma codeseg BANK4
#pragma constseg BANK4

extern __xdata uint8_t outbuf[TCP_OUTBUF_SIZE];
extern __xdata uint16_t slen;
extern __xdata uint8_t sfr_data[4];

static __code const char acl_json_header[] =
	"HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\n\r\n";

/* Keep HTTP scratch separate from the command parser's staged match/action.
 * Use XDATA for scratch and stored parameters to preserve scarce internal RAM.
 * Each response is built completely before the shared output buffer is sent. */
static __xdata uint32_t page_acl_x[5], page_acl_y[5];
static __xdata char page_acl_digits[10];

static void acl_text(__code const char *s)
{
	slen += strtox(outbuf + slen, s);
}

static void acl_number(__xdata uint32_t value)
{
	static __xdata uint8_t n;
	n = 0;
	do {
		page_acl_digits[n++] = '0' + value % 10;
		value /= 10;
	} while (value);
	while (n)
		outbuf[slen++] = page_acl_digits[--n];
}

/* JSON quoting stays local; fixed-width formatting is shared with other pages. */
static void acl_hex(uint32_t value)
{
	outbuf[slen++] = '"';
	long_to_html(value);
	outbuf[slen++] = '"';
}

static void acl_hex_reg(uint16_t addr)
{
	outbuf[slen++] = '"';
	reg_to_html_long(addr);
	outbuf[slen++] = '"';
}

static void acl_table(uint8_t type, __xdata uint8_t idx)
{
	acl_tbl_wait();
	REG_WRITE(RTL837X_TBL_CTRL, 0, idx, type, TBL_EXECUTE);
	acl_tbl_wait();
}

void send_acl(void) __banked
{
	slen = strtox(outbuf, acl_json_header);
	acl_text("{\"drop\":"); acl_number(acl_unmatch_drop);
	acl_text(",\"gpio\":"); acl_number(acl_reg_read(RTL837X_IO_MUX_SEL_2) & 15);
	acl_text(",\"polarity\":"); acl_number(acl_reg_read(RTL837X_ACL_GPIO_CTRL) & 1);
	acl_text(",\"counterMode\":"); acl_number(acl_reg_read(RTL837X_ACL_LOG_TYPE) & 0xffff);
	acl_text(",\"counterWidth\":"); acl_number(acl_reg_read(RTL837X_ACL_LOG_MODE) & 0xffff);
	acl_text(",\"hit\":[");
	acl_hex_reg(RTL837X_ACL_HIT); outbuf[slen++] = ',';
	acl_hex_reg(RTL837X_ACL_HIT + 4);
	acl_text("],\"exceeded\":[");
	for (uint8_t i = 0; i < 2; i++) {
		if (i) outbuf[slen++] = ',';
		acl_hex_reg(RTL837X_METER_EXCEED + (i << 2));
		/* Like acl show: write back the bits observed to acknowledge them. */
		reg_write_m(RTL837X_METER_EXCEED + (i << 2));
	}
	acl_text("]}");
}

/* At most four used entries: even all eight fully masked fields fit well
 * inside TCP_OUTBUF_SIZE. next is the next used 1-based rule, or zero. */
void send_acl_rules(uint8_t idx) __banked
{
	static __xdata uint8_t i, count, slot;
	static __xdata uint16_t value, care;
	static __xdata uint32_t word, mask;

	slen = strtox(outbuf, acl_json_header);
	acl_text("{\"rules\":[");
	count = 0;
	for (i = idx - 1; i < ACL_RULES; i++) {
		if (!ACL_IS_USED(i)) continue;
		if (count == 4) break;
		if (count++) outbuf[slen++] = ',';
		acl_table(TBL_ACL_RULE, i);
		for (slot = 0; slot < 5; slot++)
			page_acl_x[slot] = acl_reg_read(RTL837x_L2_DATA_OUT_A + (slot << 2));
		acl_table(TBL_ACL_RULE, 0x80 | i);
		for (slot = 0; slot < 5; slot++)
			page_acl_y[slot] = acl_reg_read(RTL837x_L2_DATA_OUT_A + (slot << 2));
		acl_text("{\"n\":"); acl_number(i + 1);
		acl_text(",\"t\":"); acl_number(acl_rule_tmpl[i]);
		acl_text(",\"ctrl\":"); acl_number(acl_reg_read(RTL837X_ACL_ACT_CTRL + (i << 2)));
		acl_text(",\"info\":[");
		/* Y's wildcard port bits mean allowed ingress ports. Preserve them. */
		acl_number((uint16_t)page_acl_y[4]); outbuf[slen++] = ',';
		acl_number(page_acl_y[4] >> 16); outbuf[slen++] = ',';
		mask = ~(page_acl_x[4] & page_acl_y[4]);
		acl_number((uint16_t)mask); outbuf[slen++] = ',';
		acl_number(mask >> 16);
		acl_text("],\"fields\":[");
		for (slot = 0; slot < 8; slot++) {
			word = page_acl_y[slot >> 1];
			mask = ~(page_acl_x[slot >> 1] & word);
			if (slot & 1) { word >>= 16; mask >>= 16; }
			care = mask;
			value = word & care;
			if (slot) outbuf[slen++] = ',';
			outbuf[slen++] = '[';
			acl_number(value); outbuf[slen++] = ',';
			acl_number(care); outbuf[slen++] = ']';
		}
		acl_text("],\"act\":[");
		acl_table(TBL_ACL_ACT, i);
		for (slot = 0; slot < 3; slot++) {
			if (slot) outbuf[slen++] = ',';
			acl_hex_reg(RTL837x_L2_DATA_OUT_A + (slot << 2));
		}
		acl_text("]}");
	}
	acl_text("],\"next\":"); acl_number(i < ACL_RULES ? i + 1 : 0);
	outbuf[slen++] = '}';
}

void send_acl_ranges(__xdata uint8_t kind) __banked
{
	static __code const uint16_t bases[] = {RTL837X_ACL_RNG_VID, RTL837X_ACL_RNG_IP, RTL837X_ACL_RNG_PORT};
	static __code const uint8_t strides[] = {4, 12, 8};
	static __xdata uint16_t addr;
	static __xdata uint32_t value, lo, hi;
	static __xdata uint8_t i;

	slen = strtox(outbuf, acl_json_header);
	outbuf[slen++] = '[';
	addr = bases[kind];
	for (i = 0; i < ACL_RANGES; i++, addr += strides[kind]) {
		value = acl_reg_read(addr);
		if (kind == ACL_RANGE_VID) {
			lo = (value >> 2) & 0xfff;
			hi = (value >> 14) & 0xfff;
		} else if (kind == ACL_RANGE_IP) {
			hi = acl_reg_read(addr + 4);
			lo = acl_reg_read(addr + 8);
		} else {
			lo = acl_reg_read(addr + 4);
			hi = lo >> 16;
			lo &= 0xffff;
		}
		if (i) outbuf[slen++] = ',';
		acl_text("{\"type\":"); acl_number(value & (kind == ACL_RANGE_IP ? 7 : 3));
		acl_text(",\"lo\":"); acl_hex(lo);
		acl_text(",\"hi\":"); acl_hex(hi);
		outbuf[slen++] = '}';
	}
	outbuf[slen++] = ']';
}

void send_acl_meters(uint8_t idx) __banked
{
	static __xdata uint8_t i, end;
	static __xdata uint32_t mode, ifg;

	slen = strtox(outbuf, acl_json_header);
	acl_text("{\"meters\":[");
	end = idx + 8;
	if (end > ACL_METERS) end = ACL_METERS;
	for (i = idx; i < end; i++) {
		if (i != idx) outbuf[slen++] = ',';
		mode = acl_reg_read(RTL837X_METER_MODE + ((i >> 5) << 2));
		ifg = acl_reg_read(RTL837X_METER_IFG + ((i >> 5) << 2));
		acl_text("{\"n\":"); acl_number(i);
		acl_text(",\"rate\":"); acl_number(acl_reg_read(RTL837X_METER_RATE + ((uint16_t)i << 2)) & 0xffffff);
		acl_text(",\"burst\":"); acl_number(acl_reg_read(RTL837X_METER_BURST + ((uint16_t)i << 2)) & 0xfffffff);
		acl_text(",\"pps\":"); acl_number((mode >> (i & 31)) & 1);
		acl_text(",\"ifg\":"); acl_number((ifg >> (i & 31)) & 1);
		outbuf[slen++] = '}';
	}
	acl_text("],\"next\":"); acl_number(end < ACL_METERS ? end : 0);
	outbuf[slen++] = '}';
}

void send_acl_fields(void) __banked
{
	static __xdata uint16_t value;

	slen = strtox(outbuf, acl_json_header);
	outbuf[slen++] = '[';
	for (uint8_t i = 0; i < ACL_SELECTORS; i++) {
		if (i) outbuf[slen++] = ',';
		value = acl_reg_read(RTL837X_ACL_FIELD_SEL + (i << 2));
		acl_text("{\"format\":"); acl_number(value & 7);
		acl_text(",\"offset\":"); acl_number((value >> 3) & 255);
		outbuf[slen++] = '}';
	}
	outbuf[slen++] = ']';
}

void send_acl_counters(void) __banked
{
	slen = strtox(outbuf, acl_json_header);
	outbuf[slen++] = '[';
	for (uint8_t i = 0; i < ACL_COUNTERS; i++) {
		if (i) outbuf[slen++] = ',';
		acl_hex_reg(RTL837X_ACL_LOG_DATA + ((uint16_t)i << 2));
	}
	outbuf[slen++] = ']';
}

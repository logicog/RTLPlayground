/*
 * LLDP (IEEE 802.1AB): send an LLDPDU on every permitted port, learn one
 * neighbour per port from the LLDPDUs trapped to the CPU, and report both on
 * the console ("lldp") and in /lldp.json.
 * Internal RAM is full: every variable, local and parameter here is __xdata.
 */
#pragma codeseg BANK3
#pragma constseg BANK3

#include "rtl837x_common.h"
#include "rtl837x_lldp.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_trap.h"
#include "rtl837x_port.h"
#include "cmd_parser.h"
#include "phy.h"
#include "uip.h"
#include "machine.h"

extern __xdata bool lldp_enabled;
extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata uint8_t outbuf[TCP_OUTBUF_SIZE];
extern __xdata uint16_t slen;
extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[];

__xdata uint16_t lldp_ticks;
__xdata uint8_t lldp_sec_ticks;
__xdata uint16_t lldp_logical_port_status;
__xdata uint8_t lldp_tlv;
__xdata struct lldp_nb lldp_nb[9];

/* Scratch of the RX parser, the TX builder and the JSON writer */
__xdata struct lldp_nb lldp_rx;
__xdata uint16_t lldp_p, lldp_end, lldp_len;
__xdata uint8_t lldp_n, lldp_type, lldp_port;
__xdata uint8_t * __xdata lldp_v;
__xdata uint8_t * __xdata lldp_tp;
__xdata struct lldp_nb * __xdata lldp_cur;

/* The firmware's memset() takes an 8 bit length */
_Static_assert(sizeof(struct lldp_nb) < 256, "struct lldp_nb too large for memset");

static __code const char * __code const lldp_tlv_names[LLDP_TX_N] = {
	"portdesc", "syscap", "mgmt", "pvid", "macphy", "mfs"
};

/* IANA MAU types by the speed code of RTL837X_REG_LINKS (10M, 100M, 1G, 500M,
 * 10G, 2.5G, 5G) at full duplex; half duplex 10M..1G copper is one less */
static __code const uint8_t lldp_mau_cu[7] = { 11, 16, 30, 0, 54, 103, 104 };
static __code const uint8_t lldp_mau_fiber[7] = { 0, 18, 22, 0, 33, 110, 112 };

static __code const char * __code const lldp_hex = "0123456789abcdef";


void lldp_init(void) __banked
{
	lldp_ticks = 0;
	lldp_sec_ticks = 0;
	lldp_enabled = 0;
	lldp_logical_port_status = 0xffff;
	lldp_tlv = LLDP_TX_ALL;
	for (lldp_port = 0; lldp_port < 9; lldp_port++)
		lldp_nb[lldp_port].ttl = 0;
}


/* ---------------------------------------------------------------- TX ---- */

struct lldp_pkt {
	struct uip_eth_addr dst;
	struct uip_eth_addr src;
	struct rtl_tag rtl_tag;
	uint16_t ether_type;
	uint8_t payload[LLDP_MAX_PAYLOAD];
};

#define LLDP_O ((__xdata struct lldp_pkt *)&uip_buf[RTL_FRAME_DESC_SIZE])

// TLV header: 7 bit type, 9 bit length; no TLV we send is 256 bytes or longer
static void lldp_tlv_hdr(__xdata uint8_t type, __xdata uint8_t len)
{
	*lldp_tp++ = type << 1;
	*lldp_tp++ = len;
}

// Organizationally specific TLV header; oui is 3 bytes
static void lldp_org_hdr(__code const char * __xdata oui, __xdata uint8_t subtype, __xdata uint8_t len)
{
	lldp_tlv_hdr(LLDP_TLV_ORG, len + 4);
	memcpyc(lldp_tp, oui, 3);
	lldp_tp += 3;
	*lldp_tp++ = subtype;
}

static void lldp_put16(__xdata uint16_t v)
{
	*lldp_tp++ = v >> 8;
	*lldp_tp++ = v;
}

static void lldp_put_str(__xdata uint8_t type, __xdata char * __xdata s, __xdata uint8_t len)
{
	if (!len)
		return;
	lldp_tlv_hdr(type, len);
	memcpy(lldp_tp, s, len);
	lldp_tp += len;
}

// Speed, duplex and autonegotiation of port lldp_port as the 802.3 MAC/PHY TLV
static void lldp_put_macphy(void)
{
	__xdata uint8_t an = 0, b;
	__xdata uint16_t v, adv = 0, mau = 0;
	__xdata bool fd = 1;

	if (!machine.is_sfp[lldp_port]) {
		phy_read(lldp_port, PHY_MMD_AN, PHY_ANEG_CTRL);
		an = (SFR_DATA_U16 & 0x1000) ? 3 : 1;	// supported, enabled
		// Advertised abilities, in the bit order of ifMauAutoNegCapAdvertisedBits
		phy_read(lldp_port, PHY_MMD_AN, PHY_ANEG_ADV);
		v = SFR_DATA_U16;
		if (v & 0x0020)
			adv |= 0x4000;	// 10BASE-T
		if (v & 0x0040)
			adv |= 0x2000;	// 10BASE-T FD
		if (v & 0x0080)
			adv |= 0x0800;	// 100BASE-TX
		if (v & 0x0100)
			adv |= 0x0400;	// 100BASE-TX FD
		phy_read(lldp_port, PHY_MMD31, PHY_MMD31_GBCR);
		v = SFR_DATA_U16;
		if (v & 0x0100)
			adv |= 0x0002;	// 1000BASE-T
		if (v & 0x0200)
			adv |= 0x0001;	// 1000BASE-T FD
		phy_read(lldp_port, PHY_MMD31, PHY_MMD31_PHYSR);
		fd = SFR_DATA_U16 & 0x0008;
	}

	reg_read_m(RTL837X_REG_LINKS_STS);
	if ((sfr_data[(lldp_port >> 3) + 1] >> (lldp_port & 7)) & 1) {
		reg_read_m(lldp_port < 8 ? RTL837X_REG_LINKS : RTL837X_REG_LINKS_89);
		b = sfr_data[3 - ((lldp_port & 7) >> 1)];
		b = (lldp_port & 1) ? b >> 4 : b & 0xf;
		if (b < 7) {
			mau = machine.is_sfp[lldp_port] ? lldp_mau_fiber[b] : lldp_mau_cu[b];
			if (b < 3 && mau && !fd)
				mau--;
		}
	}

	lldp_org_hdr("\x00\x12\x0f", 1, 5);
	*lldp_tp++ = an;
	lldp_put16(adv);
	lldp_put16(mau);
}

static __xdata uint16_t lldp_mfs(void)
{
	reg_read_m(RTL8373_REG_MAC_L2_PORT_MAX_LEN + ((uint16_t)lldp_port << 8));
	return SFR_DATA_U16 & 0x3fff;
}

// Build and send the LLDPDU of port lldp_port
static void lldp_send_port(void)
{
	memcpyc(LLDP_O->dst.addr, "\x01\x80\xc2\x00\x00\x0e", MAC_ADDR_LEN);
	memcpy(LLDP_O->src.addr, uip_ethaddr.addr, MAC_ADDR_LEN);
	LLDP_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	LLDP_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
	LLDP_O->rtl_tag.reason = 0x00;
	LLDP_O->rtl_tag.flags = HTONS(RTL_TAG_LEARN_DIS);
	LLDP_O->rtl_tag.pmask = HTONS(PORT_BIT(lldp_port));
	LLDP_O->ether_type = HTONS(LLDP_ETHERTYPE);

	lldp_tp = LLDP_O->payload;

	lldp_tlv_hdr(LLDP_TLV_CHASSIS_ID, 1 + MAC_ADDR_LEN);
	*lldp_tp++ = LLDP_CHASSIS_SUB_MAC;
	memcpy(lldp_tp, uip_ethaddr.addr, MAC_ADDR_LEN);
	lldp_tp += MAC_ADDR_LEN;

	lldp_tlv_hdr(LLDP_TLV_PORT_ID, 2);
	*lldp_tp++ = LLDP_PORT_SUB_LOCAL;
	*lldp_tp++ = '0' + machine.log_to_phys_port[lldp_port];

	lldp_tlv_hdr(LLDP_TLV_TTL, 2);
	lldp_put16(LLDP_TTL_SECONDS);

	if (lldp_tlv & LLDP_TX_PORTDESC)
		lldp_put_str(LLDP_TLV_PORT_DESC, port_names[lldp_port], strlen_x(port_names[lldp_port]));
	lldp_put_str(LLDP_TLV_SYS_NAME, hostname, strlen_x(hostname));
	lldp_len = strlen(machine.machine_name);
	lldp_tlv_hdr(LLDP_TLV_SYS_DESC, lldp_len);
	memcpyc(lldp_tp, machine.machine_name, lldp_len);
	lldp_tp += lldp_len;

	if (lldp_tlv & LLDP_TX_SYSCAP) {
		lldp_tlv_hdr(LLDP_TLV_SYS_CAP, 4);
		lldp_put16(LLDP_CAP_BRIDGE);
		lldp_put16(LLDP_CAP_BRIDGE);
	}
	if ((lldp_tlv & LLDP_TX_MGMT) && (uip_hostaddr[0] || uip_hostaddr[1])) {
		lldp_tlv_hdr(LLDP_TLV_MGMT_ADDR, 12);
		*lldp_tp++ = 5;		// address string length: subtype + IPv4
		*lldp_tp++ = 1;		// IPv4
		memcpy(lldp_tp, (__xdata uint8_t *)uip_hostaddr, 4);
		lldp_tp += 4;
		*lldp_tp++ = 1;		// interface numbering unknown
		lldp_put16(0);
		lldp_put16(0);
		*lldp_tp++ = 0;		// no OID
	}
	if (lldp_tlv & LLDP_TX_PVID) {
		lldp_org_hdr("\x00\x80\xc2", 1, 2);
		lldp_put16(port_pvid_get(lldp_port));
	}
	if (lldp_tlv & LLDP_TX_MACPHY)
		lldp_put_macphy();
	if (lldp_tlv & LLDP_TX_MFS) {
		lldp_org_hdr("\x00\x12\x0f", 4, 2);
		lldp_put16(lldp_mfs());
	}

	lldp_tlv_hdr(LLDP_TLV_END, 0);
	while (lldp_tp < LLDP_O->payload + LLDP_MIN_ETHERNET_PAYLOAD_LENGTH)
		*lldp_tp++ = 0x00;

	// Ethernet frame without FCS: addresses, RTL tag, EtherType, payload
	uip_len = lldp_tp - &uip_buf[RTL_FRAME_DESC_SIZE];
	tcpip_output();
}


/* ---------------------------------------------------------------- RX ---- */

// Copy lldp_len bytes at lldp_v as printable ASCII, '"' and '\' become '.'
static void lldp_str(__xdata char * __xdata d, __xdata uint8_t size)
{
	__xdata uint8_t i, c;

	for (i = 0; i < size - 1 && i < lldp_len; i++) {
		c = lldp_v[i];
		d[i] = (c < 0x20 || c > 0x7e || c == '"' || c == '\\') ? '.' : c;
	}
	d[i] = NUL;
}

// A chassis or port ID: text if printable, else (and always for a MAC) hex bytes
static void lldp_id(__xdata char * __xdata d, __xdata bool hex)
{
	__xdata uint8_t i, c;

	for (i = 0; i < lldp_len && i < LLDP_STR_SIZE - 1; i++) {
		c = lldp_v[i];
		if (c < 0x20 || c > 0x7e)
			hex = 1;
	}
	if (!hex) {
		lldp_str(d, LLDP_STR_SIZE);
		return;
	}
	for (i = 0; i < lldp_len && i < LLDP_STR_SIZE / 3; i++) {
		if (i)
			*d++ = ':';
		*d++ = lldp_hex[lldp_v[i] >> 4];
		*d++ = lldp_hex[lldp_v[i] & 0xf];
	}
	*d = NUL;
}

static __xdata uint16_t lldp_get16(__xdata uint8_t i)
{
	return ((uint16_t)lldp_v[i] << 8) | lldp_v[i + 1];
}

/*
 * An LLDPDU trapped to the CPU is in uip_buf. Frames are untrusted: every
 * TLV is checked against the frame end, a frame with a truncated TLV or
 * without Chassis ID, Port ID and TTL as its first three TLVs is dropped
 * and leaves the stored neighbour alone.
 */
void lldp_in(void) __banked
{
	lldp_p = TRAP_RX_BODY();
	lldp_end = uip_len;
	uip_len = 0;
	if (lldp_end < lldp_p + 2 || uip_buf[lldp_p] != (LLDP_ETHERTYPE >> 8)
	    || uip_buf[lldp_p + 1] != (LLDP_ETHERTYPE & 0xff))
		return;
	lldp_p += 2;

	// The low nibble of the RTL tag's pmask is the source port
	lldp_port = ((uint8_t)HTONS(((__xdata struct rtl_tag *)&uip_buf[12])->pmask)) & 0x0f;
	if (lldp_port < machine.min_port || lldp_port > machine.max_port)
		return;

	memset((__xdata uint8_t *)&lldp_rx, 0, sizeof(lldp_rx));
	// lldp_n counts the TLVs up to 3, enough to check the mandatory ones
	for (lldp_n = 0; lldp_p + 2 <= lldp_end; ) {
		lldp_type = uip_buf[lldp_p] >> 1;
		lldp_len = ((uint16_t)(uip_buf[lldp_p] & 1) << 8) | uip_buf[lldp_p + 1];
		lldp_p += 2;
		if (lldp_p + lldp_len > lldp_end)
			return;
		if (lldp_type == LLDP_TLV_END)
			break;
		lldp_v = &uip_buf[lldp_p];
		lldp_p += lldp_len;
		// Chassis ID, Port ID, TTL first and only once
		if (lldp_n < 3) {
			if (lldp_type != lldp_n + 1)
				return;
			lldp_n++;
		} else if (lldp_type <= LLDP_TLV_TTL) {
			return;
		}

		switch (lldp_type) {
		case LLDP_TLV_CHASSIS_ID:
		case LLDP_TLV_PORT_ID:
			if (lldp_len < 2)
				return;
			lldp_len--;
			if (lldp_type == LLDP_TLV_CHASSIS_ID) {
				lldp_rx.chassis_sub = *lldp_v++;
				lldp_id(lldp_rx.chassis, lldp_rx.chassis_sub == LLDP_CHASSIS_SUB_MAC);
			} else {
				lldp_rx.port_sub = *lldp_v++;
				lldp_id(lldp_rx.port, lldp_rx.port_sub == LLDP_PORT_SUB_MAC);
			}
			break;
		case LLDP_TLV_TTL:
			if (lldp_len < 2)
				return;
			lldp_rx.ttl = lldp_get16(0);
			break;
		case LLDP_TLV_PORT_DESC:
			lldp_str(lldp_rx.port_desc, LLDP_STR_SIZE);
			break;
		case LLDP_TLV_SYS_NAME:
			lldp_str(lldp_rx.sys_name, LLDP_STR_SIZE);
			break;
		case LLDP_TLV_SYS_DESC:
			lldp_str(lldp_rx.sys_desc, LLDP_DESC_SIZE);
			break;
		case LLDP_TLV_SYS_CAP:
			if (lldp_len >= 4) {
				lldp_rx.cap_sup = lldp_get16(0);
				lldp_rx.cap_en = lldp_get16(2);
			}
			break;
		case LLDP_TLV_MGMT_ADDR:
			// Address string length 5 (subtype + 4 bytes), subtype 1: IPv4
			if (lldp_len >= 6 && lldp_v[0] == 5 && lldp_v[1] == 1
			    && !(lldp_rx.ip[0] | lldp_rx.ip[1] | lldp_rx.ip[2] | lldp_rx.ip[3]))
				memcpy(lldp_rx.ip, lldp_v + 2, 4);
			break;
		case LLDP_TLV_ORG:
			// IEEE 802.1 (00-80-C2) subtype 1: Port VLAN ID
			if (lldp_len >= 6 && lldp_v[0] == 0x00 && lldp_v[1] == 0x80
			    && lldp_v[2] == 0xc2 && lldp_v[3] == 1)
				lldp_rx.pvid = lldp_get16(4);
			break;
		}
	}
	if (lldp_n < 3)
		return;

	// TTL 0 is a shutdown LLDPDU: it removes the neighbour
	memcpy((__xdata uint8_t *)&lldp_nb[lldp_port], (__xdata uint8_t *)&lldp_rx, sizeof(lldp_rx));
}


/* ------------------------------------------------------------- Timer ---- */

void lldp_tick(void) __banked
{
	if (++lldp_sec_ticks >= SYS_TICK_HZ) {
		lldp_sec_ticks = 0;
		// Age the neighbours, forget them when the TTL runs out or the link goes down
		reg_read_m(RTL837X_REG_LINKS_STS);
		for (lldp_port = machine.min_port; lldp_port <= machine.max_port; lldp_port++) {
			lldp_cur = &lldp_nb[lldp_port];
			if (!lldp_cur->ttl)
				continue;
			if (!((sfr_data[(lldp_port >> 3) + 1] >> (lldp_port & 7)) & 1) || ++lldp_cur->age >= lldp_cur->ttl)
				lldp_cur->ttl = 0;
		}
	}

	if (++lldp_ticks < LLDP_TX_INTERVAL_TICKS)
		return;
	lldp_ticks = 0;

	if (!lldp_enabled)
		return;
	for (lldp_port = machine.min_port; lldp_port <= machine.max_port; lldp_port++) {
		if (PORT_BIT(lldp_port) & lldp_logical_port_status)
			lldp_send_port();
	}
}


/* --------------------------------------------------------------- CLI ---- */

static uint8_t lldp_word(__xdata uint8_t w, __code const char * __xdata s)
{
	if (w >= cmd_words_len)
		return 0;
	lldp_v = &cmd_buffer[cmd_words_b[w]];
	if (!strstart(lldp_v, s))
		return 0;
	lldp_v += strlen(s);
	if (*lldp_v == ' ' || *lldp_v == NUL)
		return 1;
	return 0;
}

static void lldp_show(void)
{
	print_string("LLDP ");
	print_string(lldp_enabled ? "on" : "off");
	print_string(", TLVs:");
	for (lldp_n = 0; lldp_n < LLDP_TX_N; lldp_n++) {
		write_char(' ');
		print_string(lldp_tlv_names[lldp_n]);
		print_string((lldp_tlv >> lldp_n) & 1 ? " on" : " off");
	}
	write_char('\n');
	for (lldp_port = machine.min_port; lldp_port <= machine.max_port; lldp_port++) {
		lldp_cur = &lldp_nb[lldp_port];
		print_string("Port ");
		print_phys_port(lldp_port);
		print_string((lldp_logical_port_status >> lldp_port) & 1 ? ": send" : ": block");
		if (lldp_cur->ttl) {
			print_string(", neighbour \"");
			print_string_x(lldp_cur->sys_name);
			print_string("\" port \"");
			print_string_x(lldp_cur->port);
			print_string("\" chassis ");
			print_string_x(lldp_cur->chassis);
			print_string(" ip ");
			print_ip(lldp_cur->ip);
			print_string(" age ");
			itoa_short(lldp_cur->age);
			write_char('/');
			itoa_short(lldp_cur->ttl);
		}
		write_char('\n');
	}
}

void lldp_parse(void) __banked
{
	if (cmd_words_len == 1) {
		lldp_show();
		return;
	}
	if (cmd_words_len == 2 && lldp_word(1, "on")) {
		print_string("LLDP enabled\n");
		lldp_enabled = 1;
		trap_rma_set(RTL837X_RMA0E_CONF, RTL837X_RMA_ACT_TRAP);
		return;
	}
	if (cmd_words_len == 2 && lldp_word(1, "off")) {
		print_string("LLDP disabled\n");
		lldp_enabled = 0;
		trap_rma_set(RTL837X_RMA0E_CONF, RTL837X_RMA_ACT_FORWARD);
		return;
	}
	if (cmd_words_len == 4 && lldp_word(1, "tlv")) {
		for (lldp_n = 0; lldp_n < LLDP_TX_N; lldp_n++) {
			if (lldp_word(2, lldp_tlv_names[lldp_n])) {
				if (lldp_word(3, "on")) {
					lldp_tlv |= 1 << lldp_n;
					return;
				}
				if (lldp_word(3, "off")) {
					lldp_tlv &= ~(1 << lldp_n);
					return;
				}
				break;
			}
		}
	}
	err_status = ERR_INVALID_ARGUMENT;
	print_string("Error: lldp [on|off|tlv portdesc|syscap|mgmt|pvid|macphy|mfs on|off]\n");
}


/* -------------------------------------------------------------- JSON ---- */

/* Strings are stored JSON-safe (see lldp_str), so they are copied verbatim.
 * Worst case for 9 ports is measured by test_lldp against TCP_OUTBUF_SIZE. */
static void lldp_js(__code const char * __xdata s)
{
	slen += strtox(outbuf + slen, s);
}

static void lldp_jn(__xdata uint16_t v)
{
	__xdata uint16_t d = 10000;
	__xdata uint8_t c, z = 0;

	do {
		c = v / d;
		v -= c * d;
		if (c || z || d == 1) {
			outbuf[slen++] = '0' + c;
			z = 1;
		}
		d /= 10;
	} while (d);
}

static void lldp_jstr(__code const char * __xdata key, __xdata char * __xdata s)
{
	lldp_js(key);
	outbuf[slen++] = '"';
	while (*s)
		outbuf[slen++] = *s++;
	outbuf[slen++] = '"';
}

// Port lldp_port as a JSON object; detail adds what only the pop-ups need
static void lldp_json_port(__xdata bool detail)
{
	lldp_js("{\"p\":");
	lldp_jn(machine.log_to_phys_port[lldp_port]);
	lldp_js(",\"tx\":");
	lldp_jn((lldp_logical_port_status >> lldp_port) & 1);
	if (detail) {
		lldp_js(",\"pvid\":");
		lldp_jn(port_pvid_get(lldp_port));
		lldp_js(",\"mfs\":");
		lldp_jn(lldp_mfs());
	}
	lldp_cur = &lldp_nb[lldp_port];
	if (lldp_cur->ttl) {
		lldp_jstr(",\"nb\":{\"ch\":", lldp_cur->chassis);
		lldp_jstr(",\"pi\":", lldp_cur->port);
		lldp_jstr(",\"sn\":", lldp_cur->sys_name);
		lldp_js(",\"ip\":\"");
		if (lldp_cur->ip[0] | lldp_cur->ip[1] | lldp_cur->ip[2] | lldp_cur->ip[3]) {
			for (lldp_n = 0; lldp_n < 4; lldp_n++) {
				if (lldp_n)
					outbuf[slen++] = '.';
				lldp_jn(lldp_cur->ip[lldp_n]);
			}
		}
		lldp_js("\",\"cap\":");
		lldp_jn(lldp_cur->cap_en);
		lldp_js(",\"age\":");
		lldp_jn(lldp_cur->age);
		if (detail) {
			lldp_js(",\"cs\":");
			lldp_jn(lldp_cur->chassis_sub);
			lldp_js(",\"ps\":");
			lldp_jn(lldp_cur->port_sub);
			lldp_jstr(",\"pd\":", lldp_cur->port_desc);
			lldp_jstr(",\"sd\":", lldp_cur->sys_desc);
			lldp_js(",\"sc\":");
			lldp_jn(lldp_cur->cap_sup);
			lldp_js(",\"vid\":");
			lldp_jn(lldp_cur->pvid);
			lldp_js(",\"ttl\":");
			lldp_jn(lldp_cur->ttl);
		}
		outbuf[slen++] = '}';
	}
	outbuf[slen++] = '}';
}

/*
 * /lldp.json: settings and a neighbour summary of all ports.
 * /lldp.json?port=<n>: one port with what it sends and its full neighbour.
 */
void send_lldp(__xdata uint8_t phys_port) __banked
{
	slen = strtox(outbuf, "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\n\r\n");

	if (phys_port) {
		for (lldp_port = machine.min_port; lldp_port <= machine.max_port; lldp_port++) {
			if (machine.log_to_phys_port[lldp_port] == phys_port) {
				lldp_json_port(1);
				return;
			}
		}
		lldp_js("{}");
		return;
	}

	lldp_js("{\"on\":");
	lldp_jn(lldp_enabled);
	lldp_js(",\"tlv\":");
	lldp_jn(lldp_tlv);
	lldp_js(",\"ports\":[");
	for (lldp_port = machine.min_port; lldp_port <= machine.max_port; lldp_port++) {
		if (lldp_port != machine.min_port)
			outbuf[slen++] = ',';
		lldp_json_port(0);
	}
	lldp_js("]}");
}

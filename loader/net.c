#include <8051.h>
#include <stdint.h>
#include <stdbool.h>

#include "../rtl837x_sfr.h"
#include "../rtl837x_regs.h"
#include "../rtl837x_common.h"
#include "../phy.h"
#include "loader.h"

#define NET_MTU		1536
#define TFTP_PORT	49152

// The loader only uses the built-in PHYs of ports 4-7
#define BUILTIN_PORT_FIRST	4
#define BUILTIN_PORT_LAST	7
#define BUILTIN_PHY_MASK	0xf0

// PHY_POWER value the application writes, without PHY_POWER_OFF
#define PHY_POWER_ON		0x2058

// RTL837X_REG_PORT_CTRL value the application writes for the built-in ports
#define PORT_CTRL_BUILTIN	0x0f77

// First byte of the MAC address: locally administered, unicast
#define MAC_LOCAL		0x06

// Header the NIC expects in front of a frame to send
#define NIC_TX_HDR		8
#define NIC_TX_SEQ		0
#define NIC_TX_CSUM		1
#define NIC_TX_LEN		4
#define NIC_TX_CSUM_ALL		0x07

// Ethernet
#define ETH_DST			0
#define ETH_SRC			6
#define ETH_TYPE		12
#define ETH_HDR			14
#define ETH_TYPE_IPV4		0x0800
#define ETH_TYPE_ARP		0x0806
#define ETH_TYPE_VLAN		0x8100

// ARP for IPv4 over Ethernet
#define ARP_OPER		7
#define ARP_SHA			8
#define ARP_SPA			14
#define ARP_THA			18
#define ARP_TPA			24
#define ARP_LEN			28
#define ARP_REQUEST		1
#define ARP_REPLY		2

// IPv4, the loader only sends headers without options
#define IP_VER_IHL		0
#define IP_TOTAL_LEN		2
#define IP_FLAGS		6
#define IP_TTL			8
#define IP_PROTO		9
#define IP_CSUM			10
#define IP_SRC			12
#define IP_DST			16
#define IP_HDR			20
#define IP_VER_IHL_PLAIN	0x45
#define IP_DONT_FRAGMENT	0x40
#define IP_TTL_DEFAULT		64
#define IP_PROTO_ICMP		1
#define IP_PROTO_UDP		17

// ICMP
#define ICMP_CSUM		2
#define ICMP_HDR		8
#define ICMP_ECHO_REPLY		0
#define ICMP_ECHO_REQUEST	8

// UDP
#define UDP_SRC_PORT		0
#define UDP_DST_PORT		2
#define UDP_LEN			4
#define UDP_CSUM		6
#define UDP_HDR			8

// TFTP, RFC 1350
#define TFTP_SERVER_PORT	69
#define TFTP_OPCODE		0
#define TFTP_BLOCK		2
#define TFTP_HDR		4
#define TFTP_BLOCK_SIZE		512
#define TFTP_RRQ		1
#define TFTP_DATA_PKT		3
#define TFTP_ACK		4
#define TFTP_ERROR		5

// Data bytes of the last register access, reg_buf[0] holds bits 0-7
__xdata uint8_t reg_buf[4];
__xdata uint8_t rx_hdr[8];
__xdata uint8_t rx_buf[NET_MTU + 8];
__xdata uint8_t tx_buf[NET_MTU + 8];
__xdata uint8_t mac[6];
__xdata uint8_t ip[4] = { 192, 168, 2, 2 };
__xdata uint8_t server_ip[4];
__xdata uint8_t server_mac[6];
__xdata uint16_t server_port;
__xdata uint16_t tftp_block;
__xdata uint8_t tftp_tries;
__xdata uint8_t tx_seq;
__xdata uint16_t rx_len;
__xdata uint8_t link_state;
__xdata bool net_used;

static void reg_read_buf(uint16_t a)
{
	reg_read(a);
	reg_buf[0] = SFR_DATA_0;
	reg_buf[1] = SFR_DATA_8;
	reg_buf[2] = SFR_DATA_16;
	reg_buf[3] = SFR_DATA_24;
}

static void reg_write_buf(uint16_t a)
{
	SFR_DATA_0 = reg_buf[0];
	SFR_DATA_8 = reg_buf[1];
	SFR_DATA_16 = reg_buf[2];
	SFR_DATA_24 = reg_buf[3];
	reg_write(a);
}

static void reg_write16(uint16_t a, uint16_t v)
{
	reg_buf[0] = v;
	reg_buf[1] = v >> 8;
	reg_buf[2] = 0;
	reg_buf[3] = 0;
	reg_write_buf(a);
}

static void reg_modify_byte(uint16_t a, uint8_t n, uint8_t clr, uint8_t set)
{
	reg_read_buf(a);
	reg_buf[n] = (reg_buf[n] & ~clr) | set;
	reg_write_buf(a);
}

// Clear and set bits of register a. clr and set are 32 bit masks whose bits
// all lie in byte n of the register, so only that byte is changed.
#define REG_MODIFY(a, n, clr, set) \
	reg_modify_byte(a, n, (uint8_t)((clr) >> (8 * (n))), (uint8_t)((set) >> (8 * (n))))

static void builtin_phys_power(uint16_t v)
{
	SFR_DATA_8 = v >> 8;
	SFR_DATA_0 = v;
	SFR_SMI_PHYMASK = BUILTIN_PHY_MASK;
	SFR_SMI_REG_U16 = PHY_POWER;
	SFR_SMI_DEV = (uint8_t)(PHY_MMD31 << 3 | 2);
	SFR_EXEC_GO = SFR_EXEC_WRITE_SMI;
	while (SFR_EXEC_STATUS != 0);
}

#ifndef NIC_GO
#define NIC_GO(b, r, w)	nic_go(b, r, w)
static bool nic_go(__xdata uint8_t *b, uint16_t ring, uint8_t words)
{
	uint16_t guard = 0;

	SFR_NIC_DATA_U16LE = (uint16_t)b;
	SFR_NIC_RING_U16LE = ring;
	SFR_NIC_CTRL = words;
	while (SFR_NIC_CTRL != 0) {
		if (!++guard)
			return false;
	}
	return true;
}
#endif

// Bring up the built-in ports and the NIC for the CPU only. The sequence
// follows rtl8372_init and nic_setup of the application, limited to ports
// 4-7. Where the application has no name for a bit, the same number is used.
static void net_up(void)
{
	uint8_t p;

	REG_MODIFY(RTL837X_REG_RESET, 0, 0, 1 << RESET_NIC_BIT);
	do {
		reg_read(RTL837X_REG_RESET);
	} while (SFR_DATA_0 & (1 << RESET_NIC_BIT));

	REG_MODIFY(RTL837X_REG_SMI_CTRL, 1, 0, SMI_CTRL_MDC_ENABLE);
	timer0_start();
	for (p = 8; p; ) {
		if (tick())
			p--;
	}

	// Same MDI setting as rtl8372_init
	REG_MODIFY(RTL837X_CFG_PHY_MDI_REVERSE, 0, 0x0f, 0x0c);
	reg_read_buf(RTL837X_REG_CHIP_ID);
	if (reg_buf[1] == CHIP_ID_N_VERSION) {
		reg_read_buf(RTL837X_CFG_PHY_TX_POLARITY_SWAP);
		reg_buf[1] = PHY_TX_POLARITY_SWAP_N >> 8;
		reg_buf[0] = PHY_TX_POLARITY_SWAP_N & 0xff;
		reg_write_buf(RTL837X_CFG_PHY_TX_POLARITY_SWAP);
	}

	// Configure the ports with their PHYs powered down, as rtl8372_init does
	builtin_phys_power(PHY_POWER_ON | PHY_POWER_OFF);
	REG_MODIFY(0x5fd4, 2, 0, 0x00180000);
	for (p = BUILTIN_PORT_FIRST; p <= BUILTIN_PORT_LAST; p++)
		reg_write16(RTL837X_REG_PORT_CTRL(p), PORT_CTRL_BUILTIN);
	REG_MODIFY(0xb7c, 0, 0, 0x00000020);
	REG_MODIFY(RTL837X_REG_HW_CONF, 0, 0, 0x00000001);
	builtin_phys_power(PHY_POWER_ON);
	REG_MODIFY(RTL837X_REG_MAC_ENABLE, 2, 0,
		MAC_ENABLE_PORT(4) | MAC_ENABLE_PORT(5) | MAC_ENABLE_PORT(6) | MAC_ENABLE_PORT(7));

	// Every port only forwards to the CPU
	for (p = 0; p < 9; p++)
		reg_write16(RTL837X_PORT_ISOLATION_BASE + (p << 2), PMASK_CPU);

	// NIC, as nic_setup does it
	REG_MODIFY(RTL837X_REG_HW_CONF, 1, 0, HW_CONF_NIC_ENABLE);
	reg_write16(RTL837X_REG_NIC_RXBUFF_RX, RTL837X_NIC_RX_LAST_SLOT);
	reg_write16(RTL837X_REG_NIC_BUFFSIZE_TX, RTL837X_NIC_TX_BUFF_SIZE);
	REG_MODIFY(RTL837X_REG_RX_CTRL, 3, NIC_RX_TYPES_MASK, NIC_RX_TYPES);
	REG_MODIFY(RTL837X_REG_RX_CTRL, 2, 0x00030000, 0);
	REG_MODIFY(RTL837X_REG_TX_CTRL, 0, 0, NIC_TX_ENABLE);
	REG_MODIFY(RTL837X_REG_RX_CTRL, 0, NIC_RX_KEEP_BAD_CRC, NIC_RX_ENABLE);
	reg_write16(RTL837X_REG_CPU_TAG_AWARE_PMASK, PMASK_CPU);
	REG_MODIFY(RTL837X_REG_CPU_TAG, 0, 0, CPU_TAG_INSERT);
	REG_MODIFY(RTL837X_REG_CPU_TAG, 1, CPU_TAG_MODE_MASK, 0);
	REG_MODIFY(RTL837X_REG_MAC_FORCE_MODE + CPU_LOGICAL_PORT * 4, 0, 0, MAC_FORCE_LINK);

	// The MAC address is made from the unique ID and the lot number of the chip
	reg_read_buf(RTL837X_REG_CHIP_UUID);
	mac[0] = MAC_LOCAL;
	mac[3] = reg_buf[3] ^ reg_buf[0];
	mac[4] = reg_buf[2] ^ reg_buf[0];
	mac[5] = reg_buf[1] ^ reg_buf[0];
	reg_read_buf(RTL837X_REG_CHIP_LOT_NO);
	mac[1] = reg_buf[3] ^ reg_buf[1];
	mac[2] = reg_buf[2] ^ reg_buf[0];
}

static void copy_bytes(__xdata uint8_t *d, __xdata const uint8_t *s, uint16_t n)
{
	while (n--)
		*d++ = *s++;
}

static bool bytes_equal(__xdata const uint8_t *a, __xdata const uint8_t *b, uint8_t n)
{
	while (n--) {
		if (*a++ != *b++)
			return false;
	}
	return true;
}

static uint16_t get_be16(__xdata const uint8_t *p)
{
	return ((uint16_t)p[0] << 8) | p[1];
}

static uint16_t inet_checksum(__xdata const uint8_t *p, uint16_t n)
{
	uint16_t s = 0;
	uint16_t w;

	while (n) {
		w = (uint16_t)p[0] << 8;
		if (n > 1)
			w |= p[1];
		s += w;
		if (s < w)
			s++;
		p += 2;
		n = n > 1 ? n - 2 : 0;
	}
	return ~s;
}

static void print_dec(uint8_t v)
{
	if (v >= 100)
		write_char('0' + v / 100);
	if (v >= 10)
		write_char('0' + v / 10 % 10);
	write_char('0' + v % 10);
}

static uint8_t ip_arg(__xdata uint8_t *dst)
{
	__xdata uint8_t n[4];
	uint8_t i;
	uint8_t k;
	uint16_t v;

	while (line[lp] == ' ')
		lp++;
	if (!line[lp])
		return 1;
	for (i = 0; i < 4; i++) {
		v = 0;
		k = 0;
		while (line[lp] >= '0' && line[lp] <= '9' && k < 3) {
			v = v * 10 + line[lp++] - '0';
			k++;
		}
		if (!k || v > 255 || line[lp] != (i < 3 ? '.' : 0))
			return 0;
		lp++;
		n[i] = v;
	}
	copy_bytes(dst, n, 4);
	return 2;
}

static void print_ip(__xdata const uint8_t *a)
{
	uint8_t i;

	for (i = 0; i < 4; i++) {
		print_dec(a[i]);
		write_char(i < 3 ? '.' : '\n');
	}
}

static void set_ip(__xdata uint8_t *dst)
{
	if (ip_arg(dst))
		print_ip(dst);
	else
		print_string("?\n");
}

void ipaddr(void)
{
	set_ip(ip);
}

void serverip(void)
{
	set_ip(server_ip);
}

// Both frame types the loader sends, IPv4 and ARP, have 0x08 as high byte
static __xdata uint8_t *eth_header(__xdata const uint8_t *dst, uint8_t type_low)
{
	copy_bytes(tx_buf + NIC_TX_HDR + ETH_DST, dst, 6);
	copy_bytes(tx_buf + NIC_TX_HDR + ETH_SRC, mac, 6);
	tx_buf[NIC_TX_HDR + ETH_TYPE] = ETH_TYPE_IPV4 >> 8;
	tx_buf[NIC_TX_HDR + ETH_TYPE + 1] = type_low;
	return tx_buf + NIC_TX_HDR + ETH_HDR;
}

static void nic_send(uint16_t len)
{
	uint16_t r;

	tx_buf[NIC_TX_SEQ] = tx_seq++;
	tx_buf[NIC_TX_CSUM] = NIC_TX_CSUM_ALL;
	tx_buf[2] = 0;
	tx_buf[3] = 0;
	tx_buf[NIC_TX_LEN] = len;
	tx_buf[NIC_TX_LEN + 1] = len >> 8;
	tx_buf[6] = 0;
	tx_buf[7] = 0;
	reg_read_buf(RTL837X_REG_CPU_TX_CURR_PKT);
	r = (((uint16_t)reg_buf[1] << 8 | reg_buf[0]) << 3) | 0x8000;
	NIC_GO(tx_buf, r, (len + 0xf) >> 3);
	reg_read_buf(RTL837X_REG_NIC_TX_CURR_PKT);
	reg_write16(RTL837X_REG_NIC_TXCMD, 1);
}

static void arp_reply(__xdata uint8_t *f, uint16_t n)
{
	__xdata uint8_t *t;

	if (n < ARP_LEN || f[ARP_OPER] != ARP_REQUEST || !bytes_equal(f + ARP_TPA, ip, 4))
		return;
	t = eth_header(rx_buf + ETH_SRC, ETH_TYPE_ARP & 0xff);
	t[0] = 0x00;
	t[1] = 0x01;
	t[2] = ETH_TYPE_IPV4 >> 8;
	t[3] = ETH_TYPE_IPV4 & 0xff;
	t[4] = 6;
	t[5] = 4;
	t[6] = 0;
	t[ARP_OPER] = ARP_REPLY;
	copy_bytes(t + ARP_SHA, mac, 6);
	copy_bytes(t + ARP_SPA, ip, 4);
	copy_bytes(t + ARP_THA, f + ARP_SHA, 10);
	nic_send(ETH_HDR + ARP_LEN);
}

static void icmp_echo_reply(__xdata uint8_t *f, uint16_t n)
{
	__xdata uint8_t *t;
	uint8_t hl = (f[IP_VER_IHL] & 0x0f) << 2;
	uint16_t tl;
	uint16_t c;

	if (n < IP_HDR + ICMP_HDR || (f[IP_VER_IHL] >> 4) != 4 || f[IP_PROTO] != IP_PROTO_ICMP || !bytes_equal(f + IP_DST, ip, 4))
		return;
	tl = get_be16(f + IP_TOTAL_LEN);
	if (hl < IP_HDR || tl > n || tl < hl + ICMP_HDR || f[hl] != ICMP_ECHO_REQUEST)
		return;
	t = eth_header(rx_buf + ETH_SRC, ETH_TYPE_IPV4 & 0xff);
	copy_bytes(t, f, tl);
	t[IP_TTL] = IP_TTL_DEFAULT;
	copy_bytes(t + IP_SRC, f + IP_DST, 4);
	copy_bytes(t + IP_DST, f + IP_SRC, 4);
	t[IP_CSUM] = 0;
	t[IP_CSUM + 1] = 0;
	c = inet_checksum(t, hl);
	t[IP_CSUM] = c >> 8;
	t[IP_CSUM + 1] = c;
	t[hl] = ICMP_ECHO_REPLY;
	t[hl + ICMP_CSUM] = 0;
	t[hl + ICMP_CSUM + 1] = 0;
	c = inet_checksum(t + hl, tl - hl);
	t[hl + ICMP_CSUM] = c >> 8;
	t[hl + ICMP_CSUM + 1] = c;
	nic_send(ETH_HDR + tl);
}

static bool nic_receive(void)
{
	uint16_t r;
	bool ok;

	reg_read_buf(RTL837X_REG_NIC_RX_BUFF_DATA);
	if (!(reg_buf[0] | reg_buf[1]))
		return false;
	reg_read_buf(RTL837X_REG_CPU_RX_CURR_PKT);
	r = ((uint16_t)reg_buf[1] << 8 | reg_buf[0]) << 3;
	ok = r < RTL837X_NIC_RX_RING_BYTES && NIC_GO(rx_hdr, r, 1);
	if (ok) {
		rx_len = rx_hdr[4] | ((uint16_t)rx_hdr[5] << 8);
		r += 8;
		if (r == RTL837X_NIC_RX_RING_BYTES)
			r = 0;
		ok = rx_len >= ETH_HDR && rx_len <= NET_MTU && NIC_GO(rx_buf, r, (rx_len + 7) >> 3);
	}
	reg_write16(RTL837X_REG_NIC_RXCMD, 1);
	if (!ok)
		print_string("rx error\n");
	return ok;
}

static void handle_frame(void)
{
	uint8_t o = ETH_TYPE;
	uint16_t t = get_be16(rx_buf + ETH_TYPE);

	if (t == RTL_FRAME_TAG_ID) {
		o += (uint8_t)RTL_TAG_SIZE;
		t = get_be16(rx_buf + o);
	}
	if (t == ETH_TYPE_VLAN) {
		o += (uint8_t)VLAN_TAG_SIZE;
		t = get_be16(rx_buf + o);
	}
	print_string("rx port ");
	print_byte(rx_hdr[3] & 0x0f);
	print_string(" type ");
	print_byte(t >> 8);
	print_byte(t);
	write_char('\n');
	o += 2;
	if (rx_len < o)
		return;
	if (t == ETH_TYPE_ARP)
		arp_reply(rx_buf + o, rx_len - o);
	else if (t == ETH_TYPE_IPV4)
		icmp_echo_reply(rx_buf + o, rx_len - o);
}

static void arp_request(void)
{
	__xdata uint8_t *t;
	uint8_t i;

	for (i = 0; i < 6; i++)
		server_mac[i] = 0xff;
	t = eth_header(server_mac, ETH_TYPE_ARP & 0xff);
	t[0] = 0x00;
	t[1] = 0x01;
	t[2] = ETH_TYPE_IPV4 >> 8;
	t[3] = ETH_TYPE_IPV4 & 0xff;
	t[4] = 6;
	t[5] = 4;
	t[6] = 0;
	t[ARP_OPER] = ARP_REQUEST;
	copy_bytes(t + ARP_SHA, mac, 6);
	copy_bytes(t + ARP_SPA, ip, 4);
	for (i = ARP_THA; i < ARP_TPA; i++)
		t[i] = 0;
	copy_bytes(t + ARP_TPA, server_ip, 4);
	nic_send(ETH_HDR + ARP_LEN);
}

static void udp_send(uint16_t port, uint16_t n)
{
	__xdata uint8_t *t = tx_buf + NIC_TX_HDR + ETH_HDR;
	__xdata uint8_t *u = t + IP_HDR;
	uint16_t c;

	eth_header(server_mac, ETH_TYPE_IPV4 & 0xff);
	t[IP_VER_IHL] = IP_VER_IHL_PLAIN;
	t[1] = 0;
	t[IP_TOTAL_LEN] = (IP_HDR + UDP_HDR + n) >> 8;
	t[IP_TOTAL_LEN + 1] = IP_HDR + UDP_HDR + n;
	t[4] = 0;
	t[5] = 0;
	t[IP_FLAGS] = IP_DONT_FRAGMENT;
	t[IP_FLAGS + 1] = 0;
	t[IP_TTL] = IP_TTL_DEFAULT;
	t[IP_PROTO] = IP_PROTO_UDP;
	t[IP_CSUM] = 0;
	t[IP_CSUM + 1] = 0;
	copy_bytes(t + IP_SRC, ip, 4);
	copy_bytes(t + IP_DST, server_ip, 4);
	c = inet_checksum(t, IP_HDR);
	t[IP_CSUM] = c >> 8;
	t[IP_CSUM + 1] = c;
	u[UDP_SRC_PORT] = TFTP_PORT >> 8;
	u[UDP_SRC_PORT + 1] = TFTP_PORT & 0xff;
	u[UDP_DST_PORT] = port >> 8;
	u[UDP_DST_PORT + 1] = port;
	u[UDP_LEN] = (UDP_HDR + n) >> 8;
	u[UDP_LEN + 1] = UDP_HDR + n;
	u[UDP_CSUM] = 0;
	u[UDP_CSUM + 1] = 0;
	nic_send(ETH_HDR + IP_HDR + UDP_HDR + n);
}

static void tftp_send(void)
{
	__xdata uint8_t *t = tx_buf + NIC_TX_HDR + ETH_HDR + IP_HDR + UDP_HDR;
	uint8_t i;

	if (server_mac[0] == 0xff && server_mac[1] == 0xff) {
		arp_request();
		return;
	}
	t[TFTP_OPCODE] = 0;
	if (!tftp_block) {
		t[TFTP_OPCODE + 1] = TFTP_RRQ;
		for (i = 0; line[lp + i]; i++)
			t[2 + i] = line[lp + i];
		t[2 + i] = 0;
		t[3 + i] = 'o';
		t[4 + i] = 'c';
		t[5 + i] = 't';
		t[6 + i] = 'e';
		t[7 + i] = 't';
		t[8 + i] = 0;
		udp_send(TFTP_SERVER_PORT, 9 + i);
		return;
	}
	t[TFTP_OPCODE + 1] = TFTP_ACK;
	t[TFTP_BLOCK] = tftp_block >> 8;
	t[TFTP_BLOCK + 1] = tftp_block;
	udp_send(server_port, TFTP_HDR);
}

static uint8_t tftp_frame(void)
{
	uint8_t o = ETH_TYPE;
	uint16_t t = get_be16(rx_buf + ETH_TYPE);
	__xdata uint8_t *f;
	__xdata uint8_t *u;
	uint16_t n;

	if (t == RTL_FRAME_TAG_ID) {
		o += (uint8_t)RTL_TAG_SIZE;
		t = get_be16(rx_buf + o);
	}
	if (t == ETH_TYPE_VLAN) {
		o += (uint8_t)VLAN_TAG_SIZE;
		t = get_be16(rx_buf + o);
	}
	o += 2;
	if (rx_len < o)
		return 0;
	f = rx_buf + o;
	n = rx_len - o;
	if (t == ETH_TYPE_ARP) {
		if (n >= ARP_LEN && f[ARP_OPER] == ARP_REPLY && bytes_equal(f + ARP_SPA, server_ip, 4)
		    && server_mac[0] == 0xff && server_mac[1] == 0xff) {
			copy_bytes(server_mac, f + ARP_SHA, 6);
			tftp_send();
			return 1;
		}
		arp_reply(f, n);
		return 0;
	}
	if (t != ETH_TYPE_IPV4)
		return 0;
	if (n < IP_HDR + UDP_HDR + TFTP_HDR || f[IP_PROTO] != IP_PROTO_UDP || (f[IP_VER_IHL] & 0x0f) != 5
	    || !bytes_equal(f + IP_SRC, server_ip, 4) || !bytes_equal(f + IP_DST, ip, 4)) {
		icmp_echo_reply(f, n);
		return 0;
	}
	u = f + IP_HDR;
	if (get_be16(u + UDP_DST_PORT) != TFTP_PORT || get_be16(u + UDP_LEN) < UDP_HDR + TFTP_HDR
	    || get_be16(u + UDP_LEN) > n - IP_HDR)
		return 0;
	n = get_be16(u + UDP_LEN) - UDP_HDR - TFTP_HDR;
	if (tftp_block && get_be16(u + UDP_SRC_PORT) != server_port)
		return 0;
	if (u[UDP_HDR + TFTP_OPCODE + 1] == TFTP_ERROR) {
		print_string("\nTFTP error: ");
		for (u += UDP_HDR + TFTP_HDR; n-- && *u; u++)
			write_char(*u >= 0x20 && *u < 0x7f ? *u : '?');
		write_char('\n');
		return 3;
	}
	if (u[UDP_HDR + TFTP_OPCODE + 1] != TFTP_DATA_PKT)
		return 0;
	if (get_be16(u + UDP_HDR + TFTP_BLOCK) != tftp_block + 1)
		return 0;
	server_port = get_be16(u + UDP_SRC_PORT);
	if (!up_add(u + UDP_HDR + TFTP_HDR, n)) {
		print_string("\nImage too large or not in 128 byte steps\n");
		return 3;
	}
	tftp_block++;
	tftp_send();
	if (!(tftp_block & 63))
		write_char('#');
	return n < TFTP_BLOCK_SIZE ? 2 : 1;
}

void tftp(void)
{
	uint8_t r;
	uint8_t n = 0;

	while (line[lp] == ' ')
		lp++;
	if (!line[lp]) {
		print_string("?\n");
		return;
	}
	net_used = true;
	net_up();
	tftp_block = 0;
	tftp_tries = 0;
	server_mac[0] = 0xff;
	server_mac[1] = 0xff;
	up_begin();
	print_string("TFTP from ");
	print_ip(server_ip);
	tftp_send();
	timer0_start();
	while (1) {
		if (RI) {
			RI = 0;
			print_string("\nStopped\n");
			return;
		}
		if (nic_receive()) {
			r = tftp_frame();
			if (r == 3)
				return;
			if (r) {
				tftp_tries = 0;
				n = 0;
			}
			if (r == 2) {
				print_string("\nReceived\n");
				up_end();
				return;
			}
		}
		if (tick() && ++n == TICKS_1S) {
			n = 0;
			if (++tftp_tries > 10) {
				print_string("\nTFTP timeout\n");
				return;
			}
			tftp_send();
		}
	}
}

void net(void)
{
	uint8_t i;
	uint8_t n = 0;

	if (!ip_arg(ip)) {
		print_string("?\n");
		return;
	}
	net_used = true;
	net_up();
	print_string("MAC ");
	for (i = 0; i < 6; i++) {
		print_byte(mac[i]);
		write_char(i < 5 ? ':' : ' ');
	}
	print_string("IP ");
	print_ip(ip);
	print_string("Built-in ports 4-7 up, any key stops\n");
	link_state = 0xff;
	timer0_start();
	while (1) {
		while (!RI && nic_receive())
			handle_frame();
		if (RI) {
			RI = 0;
			return;
		}
		if (tick()) {
			if (!n--) {
				n = TICKS_1S;
				reg_read_buf(RTL837X_REG_LINKS_STS);
				if ((reg_buf[2] & 0xf0) != link_state) {
					link_state = reg_buf[2] & 0xf0;
					print_string("links ");
					print_byte(link_state);
					write_char('\n');
				}
			}
		}
	}
}

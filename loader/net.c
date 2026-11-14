#include <8051.h>
#include <stdint.h>
#include <stdbool.h>

#include "../rtl837x_sfr.h"
#include "../rtl837x_regs.h"
#include "loader.h"

#define NET_MTU		1536
#define TFTP_PORT	49152


__xdata uint8_t rb[4];
__xdata uint8_t rxh[8];
__xdata uint8_t rxb[NET_MTU + 8];
__xdata uint8_t txb[NET_MTU + 8];
__xdata uint8_t mac[6];
__xdata uint8_t ip[4] = { 192, 168, 2, 2 };
__xdata uint8_t srv[4];
__xdata uint8_t srv_mac[6];
__xdata uint16_t srv_port;
__xdata uint16_t tf_blk;
__xdata uint8_t tf_tries;
__xdata uint8_t seq;
__xdata uint16_t rlen;
__xdata uint8_t links;
__xdata bool net_used;

static void rd(uint16_t a)
{
	reg_read(a);
	rb[0] = SFR_DATA_0;
	rb[1] = SFR_DATA_8;
	rb[2] = SFR_DATA_16;
	rb[3] = SFR_DATA_24;
}

static void wr(uint16_t a)
{
	SFR_DATA_0 = rb[0];
	SFR_DATA_8 = rb[1];
	SFR_DATA_16 = rb[2];
	SFR_DATA_24 = rb[3];
	reg_write(a);
}

static void reg16(uint16_t a, uint16_t v)
{
	rb[0] = v;
	rb[1] = v >> 8;
	rb[2] = 0;
	rb[3] = 0;
	wr(a);
}

static void reg_bits(uint16_t a, uint8_t n, uint8_t clr, uint8_t set)
{
	rd(a);
	rb[n] = (rb[n] & ~clr) | set;
	wr(a);
}

static void phy_wr(uint16_t v)
{
	SFR_DATA_8 = v >> 8;
	SFR_DATA_0 = v;
	SFR_SMI_PHYMASK = 0xf0;
	SFR_SMI_REG_U16 = 0xa610;
	SFR_SMI_DEV = 0xfa;
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

static void net_up(void)
{
	uint8_t p;

	reg_bits(RTL837X_REG_RESET, 0, 0, 1 << RESET_NIC_BIT);
	do {
		reg_read(RTL837X_REG_RESET);
	} while (SFR_DATA_0 & (1 << RESET_NIC_BIT));

	reg_bits(RTL837X_REG_SMI_CTRL, 1, 0, 0x70);
	timer0_start();
	for (p = 8; p; ) {
		if (tick())
			p--;
	}
	reg_bits(RTL837X_CFG_PHY_MDI_REVERSE, 0, 0x0f, 0x0c);
	rd(RTL837X_REG_CHIP_ID);
	if (rb[1] == 0x70) {
		rd(RTL837X_CFG_PHY_TX_POLARITY_SWAP);
		rb[1] = 0x59;
		rb[0] = 0x6a;
		wr(RTL837X_CFG_PHY_TX_POLARITY_SWAP);
	}
	phy_wr(0x2858);
	reg_bits(0x5fd4, 2, 0, 0x18);
	for (p = 4; p < 8; p++)
		reg16(0x1238 + ((uint16_t)p << 8), 0xf77);
	reg_bits(0xb7c, 0, 0, 0x20);
	reg_bits(RTL837X_REG_HW_CONF, 0, 0, 0x01);
	phy_wr(0x2058);
	reg_bits(0x632c, 2, 0, 0x0f);
	for (p = 0; p < 9; p++)
		reg16(RTL837X_PORT_ISOLATION_BASE + (p << 2), 0x200);

	reg_bits(RTL837X_REG_HW_CONF, 1, 0, 0x10);
	reg16(RTL837X_REG_NIC_RXBUFF_RX, RTL837X_NIC_RX_LAST_SLOT);
	reg16(RTL837X_REG_NIC_BUFFSIZE_TX, 0x7fe);
	reg_bits(RTL837X_REG_RX_CTRL, 3, 0xff, 0x04);
	reg_bits(RTL837X_REG_RX_CTRL, 2, 0x03, 0);
	reg_bits(RTL837X_REG_TX_CTRL, 0, 0, 0x01);
	reg_bits(RTL837X_REG_RX_CTRL, 0, 0x04, 0x01);
	reg16(RTL837X_REG_CPU_TAG_AWARE_PMASK, 0x200);
	reg_bits(RTL837X_REG_CPU_TAG, 0, 0, 0x01);
	reg_bits(RTL837X_REG_CPU_TAG, 1, 0x03, 0);
	reg_bits(RTL837X_REG_MAC_FORCE_MODE + 9 * 4, 0, 0, 0x03);

	rd(RTL837X_REG_CHIP_UUID);
	mac[0] = 0x06;
	mac[3] = rb[3] ^ rb[0];
	mac[4] = rb[2] ^ rb[0];
	mac[5] = rb[1] ^ rb[0];
	rd(RTL837X_REG_CHIP_LOT_NO);
	mac[1] = rb[3] ^ rb[1];
	mac[2] = rb[2] ^ rb[0];
}

static void cp(__xdata uint8_t *d, __xdata const uint8_t *s, uint16_t n)
{
	while (n--)
		*d++ = *s++;
}

static bool same(__xdata const uint8_t *a, __xdata const uint8_t *b, uint8_t n)
{
	while (n--) {
		if (*a++ != *b++)
			return false;
	}
	return true;
}

static uint16_t be16(__xdata const uint8_t *p)
{
	return ((uint16_t)p[0] << 8) | p[1];
}

static uint16_t csum(__xdata const uint8_t *p, uint16_t n)
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
	cp(dst, n, 4);
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
	set_ip(srv);
}

static __xdata uint8_t *eth(__xdata const uint8_t *dst, uint8_t type)
{
	cp(txb + 8, dst, 6);
	cp(txb + 14, mac, 6);
	txb[20] = 0x08;
	txb[21] = type;
	return txb + 22;
}

static void send(uint16_t len)
{
	uint16_t r;

	txb[0] = seq++;
	txb[1] = 0x07;
	txb[2] = 0;
	txb[3] = 0;
	txb[4] = len;
	txb[5] = len >> 8;
	txb[6] = 0;
	txb[7] = 0;
	rd(RTL837X_REG_CPU_TX_CURR_PKT);
	r = (((uint16_t)rb[1] << 8 | rb[0]) << 3) | 0x8000;
	NIC_GO(txb, r, (len + 0xf) >> 3);
	rd(RTL837X_REG_NIC_TX_CURR_PKT);
	reg16(RTL837X_REG_NIC_TXCMD, 1);
}

static void arp(__xdata uint8_t *f, uint16_t n)
{
	__xdata uint8_t *t;

	if (n < 28 || f[7] != 1 || !same(f + 24, ip, 4))
		return;
	t = eth(rxb + 6, 0x06);
	t[0] = 0x00;
	t[1] = 0x01;
	t[2] = 0x08;
	t[3] = 0x00;
	t[4] = 6;
	t[5] = 4;
	t[6] = 0;
	t[7] = 2;
	cp(t + 8, mac, 6);
	cp(t + 14, ip, 4);
	cp(t + 18, f + 8, 10);
	send(42);
}

static void icmp(__xdata uint8_t *f, uint16_t n)
{
	__xdata uint8_t *t;
	uint8_t hl = (f[0] & 0x0f) << 2;
	uint16_t tl;
	uint16_t c;

	if (n < 28 || (f[0] >> 4) != 4 || f[9] != 1 || !same(f + 16, ip, 4))
		return;
	tl = be16(f + 2);
	if (hl < 20 || tl > n || tl < hl + 8 || f[hl] != 8)
		return;
	t = eth(rxb + 6, 0x00);
	cp(t, f, tl);
	t[8] = 64;
	cp(t + 12, f + 16, 4);
	cp(t + 16, f + 12, 4);
	t[10] = 0;
	t[11] = 0;
	c = csum(t, hl);
	t[10] = c >> 8;
	t[11] = c;
	t[hl] = 0;
	t[hl + 2] = 0;
	t[hl + 3] = 0;
	c = csum(t + hl, tl - hl);
	t[hl + 2] = c >> 8;
	t[hl + 3] = c;
	send(14 + tl);
}

static bool rx(void)
{
	uint16_t r;
	bool ok;

	rd(RTL837X_REG_NIC_RX_BUFF_DATA);
	if (!(rb[0] | rb[1]))
		return false;
	rd(RTL837X_REG_CPU_RX_CURR_PKT);
	r = ((uint16_t)rb[1] << 8 | rb[0]) << 3;
	ok = r < RTL837X_NIC_RX_RING_BYTES && NIC_GO(rxh, r, 1);
	if (ok) {
		rlen = rxh[4] | ((uint16_t)rxh[5] << 8);
		r += 8;
		if (r == RTL837X_NIC_RX_RING_BYTES)
			r = 0;
		ok = rlen >= 14 && rlen <= NET_MTU && NIC_GO(rxb, r, (rlen + 7) >> 3);
	}
	reg16(RTL837X_REG_NIC_RXCMD, 1);
	if (!ok)
		print_string("rx error\n");
	return ok;
}

static void handle(void)
{
	uint8_t o = 12;
	uint16_t t = be16(rxb + 12);

	if (t == 0x8899) {
		o += 8;
		t = be16(rxb + o);
	}
	if (t == 0x8100) {
		o += 4;
		t = be16(rxb + o);
	}
	print_string("rx port ");
	print_byte(rxh[3] & 0x0f);
	print_string(" type ");
	print_byte(t >> 8);
	print_byte(t);
	write_char('\n');
	o += 2;
	if (rlen < o)
		return;
	if (t == 0x0806)
		arp(rxb + o, rlen - o);
	else if (t == 0x0800)
		icmp(rxb + o, rlen - o);
}

static void arp_ask(void)
{
	__xdata uint8_t *t;
	uint8_t i;

	for (i = 0; i < 6; i++)
		srv_mac[i] = 0xff;
	t = eth(srv_mac, 0x06);
	t[0] = 0x00;
	t[1] = 0x01;
	t[2] = 0x08;
	t[3] = 0x00;
	t[4] = 6;
	t[5] = 4;
	t[6] = 0;
	t[7] = 1;
	cp(t + 8, mac, 6);
	cp(t + 14, ip, 4);
	for (i = 18; i < 24; i++)
		t[i] = 0;
	cp(t + 24, srv, 4);
	send(42);
}

static void udp_send(uint16_t port, uint16_t n)
{
	__xdata uint8_t *t = txb + 22;
	uint16_t c;

	eth(srv_mac, 0x00);
	t[0] = 0x45;
	t[1] = 0;
	t[2] = (28 + n) >> 8;
	t[3] = 28 + n;
	t[4] = 0;
	t[5] = 0;
	t[6] = 0x40;
	t[7] = 0;
	t[8] = 64;
	t[9] = 17;
	t[10] = 0;
	t[11] = 0;
	cp(t + 12, ip, 4);
	cp(t + 16, srv, 4);
	c = csum(t, 20);
	t[10] = c >> 8;
	t[11] = c;
	t[20] = TFTP_PORT >> 8;
	t[21] = TFTP_PORT & 0xff;
	t[22] = port >> 8;
	t[23] = port;
	t[24] = (8 + n) >> 8;
	t[25] = 8 + n;
	t[26] = 0;
	t[27] = 0;
	send(42 + n);
}

static void tftp_send(void)
{
	__xdata uint8_t *t = txb + 50;
	uint8_t i;

	if (srv_mac[0] == 0xff && srv_mac[1] == 0xff) {
		arp_ask();
		return;
	}
	t[0] = 0;
	if (!tf_blk) {
		t[1] = 1;
		for (i = 0; line[lp + i]; i++)
			t[2 + i] = line[lp + i];
		t[2 + i] = 0;
		t[3 + i] = 'o';
		t[4 + i] = 'c';
		t[5 + i] = 't';
		t[6 + i] = 'e';
		t[7 + i] = 't';
		t[8 + i] = 0;
		udp_send(69, 9 + i);
		return;
	}
	t[1] = 4;
	t[2] = tf_blk >> 8;
	t[3] = tf_blk;
	udp_send(srv_port, 4);
}

static uint8_t tftp_frame(void)
{
	uint8_t o = 12;
	uint16_t t = be16(rxb + 12);
	__xdata uint8_t *f;
	__xdata uint8_t *u;
	uint16_t n;

	if (t == 0x8899) {
		o += 8;
		t = be16(rxb + o);
	}
	if (t == 0x8100) {
		o += 4;
		t = be16(rxb + o);
	}
	o += 2;
	if (rlen < o)
		return 0;
	f = rxb + o;
	n = rlen - o;
	if (t == 0x0806) {
		if (n >= 28 && f[7] == 2 && same(f + 14, srv, 4) && srv_mac[0] == 0xff && srv_mac[1] == 0xff) {
			cp(srv_mac, f + 8, 6);
			tftp_send();
			return 1;
		}
		arp(f, n);
		return 0;
	}
	if (t != 0x0800)
		return 0;
	if (n < 32 || f[9] != 17 || (f[0] & 0x0f) != 5 || !same(f + 12, srv, 4) || !same(f + 16, ip, 4)) {
		icmp(f, n);
		return 0;
	}
	u = f + 20;
	if (be16(u + 2) != TFTP_PORT || be16(u + 4) < 12 || be16(u + 4) > n - 20)
		return 0;
	n = be16(u + 4) - 12;
	if (tf_blk && be16(u) != srv_port)
		return 0;
	if (u[9] == 5) {
		print_string("\nTFTP error: ");
		for (u += 12; n-- && *u; u++)
			write_char(*u >= 0x20 && *u < 0x7f ? *u : '?');
		write_char('\n');
		return 3;
	}
	if (u[9] != 3)
		return 0;
	if (be16(u + 10) != tf_blk + 1)
		return 0;
	srv_port = be16(u);
	if (!up_add(u + 12, n)) {
		print_string("\nImage too large or not in 128 byte steps\n");
		return 3;
	}
	tf_blk++;
	tftp_send();
	if (!(tf_blk & 63))
		write_char('#');
	return n < 512 ? 2 : 1;
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
	tf_blk = 0;
	tf_tries = 0;
	srv_mac[0] = 0xff;
	srv_mac[1] = 0xff;
	up_begin();
	print_string("TFTP from ");
	print_ip(srv);
	tftp_send();
	timer0_start();
	while (1) {
		if (RI) {
			RI = 0;
			print_string("\nStopped\n");
			return;
		}
		if (rx()) {
			r = tftp_frame();
			if (r == 3)
				return;
			if (r) {
				tf_tries = 0;
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
			if (++tf_tries > 10) {
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
	links = 0xff;
	timer0_start();
	while (1) {
		while (!RI && rx())
			handle();
		if (RI) {
			RI = 0;
			return;
		}
		if (tick()) {
			if (!n--) {
				n = TICKS_1S;
				rd(RTL837X_REG_LINKS_STS);
				if ((rb[2] & 0xf0) != links) {
					links = rb[2] & 0xf0;
					print_string("links ");
					print_byte(links);
					write_char('\n');
				}
			}
		}
	}
}

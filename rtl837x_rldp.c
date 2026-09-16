#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_rldp.h"
#include "uip.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata struct uip_eth_addr uip_ethaddr;

__xdata uint8_t rldp_on;
__xdata uint16_t rldp_off_mask;
__xdata uint8_t rldp_block[10];
__xdata uint8_t rldp_level[10];
__xdata uint16_t rldp_fwd_mask;
__xdata uint16_t rldp_tx_mask;


static uint8_t rldp_link(uint8_t port)
{
	reg_read_m(RTL837X_REG_LINKS_STS);
	return (sfr_data[(port >> 3) + 1] >> (port & 7)) & 1;
}


static void rldp_mac(uint8_t port, __xdata uint8_t on)
{
	__xdata uint16_t reg = RTL837X_MAC_L2_PORT_CTRL + ((uint16_t)port << 8);

	reg_read_m(reg);
	if (on)
		sfr_data[3] |= MAC_L2_PORT_TX_RX_EN;
	else
		sfr_data[3] &= ~MAC_L2_PORT_TX_RX_EN;
	reg_write_m(reg);
}


static void rldp_read_fwd(void)
{
	__xdata uint8_t p, st;

	reg_read_m(RTL837X_MSTP_STATES);
	rldp_fwd_mask = 0;
	for (p = machine.min_port; p <= machine.max_port; p++) {
		st = sfr_data[3 - (p >> 2)] >> ((p & 3) << 1);
		if ((st & 3) == 3)
			rldp_fwd_mask |= ((uint16_t)1) << p;
	}
}


static void rldp_apply(void)
{
	__xdata uint16_t pmask = rldp_fwd_mask & ~rldp_off_mask;

	if (!rldp_on)
		pmask = 0;
	if (pmask != rldp_tx_mask) {
		rldp_tx_mask = pmask;
		sfr_data[0] = 0;
		sfr_data[1] = 0;
		sfr_data[2] = pmask >> 8;
		sfr_data[3] = pmask;
		reg_write_m(RTL837X_RLDP_TX_PMSK);
	}

	reg_read_m(RTL837X_RLDP_RLPP);
	sfr_data[3] &= ~(RLDP_CTRL_EN | RLDP_CTRL_COMP_ID);
	sfr_data[3] |= RLDP_CTRL_PERIODIC;
	if (rldp_on)
		sfr_data[3] |= RLDP_CTRL_EN;
	reg_write_m(RTL837X_RLDP_RLPP);
}


static void rldp_open(uint8_t port)
{
	if (!rldp_block[port])
		return;
	rldp_block[port] = 0;
	rldp_mac(port, 1);
	print_string("rldp: port ");
	write_char('0' + machine.log_to_phys_port[port]);
	print_string(" opened\n");
}


void rldp_init(void) __banked
{
	__xdata uint8_t p;

	rldp_on = 0;
	rldp_off_mask = 0;
	rldp_fwd_mask = 0;
	rldp_tx_mask = 0xffff;
	for (p = 0; p < 10; p++) {
		rldp_block[p] = 0;
		rldp_level[p] = 0;
	}
	rldp_apply();
}


void rldp_enable(uint8_t on) __banked
{
	__xdata uint8_t p;

	rldp_on = on;
	if (on) {
		sfr_data[0] = uip_ethaddr.addr[2];
		sfr_data[1] = uip_ethaddr.addr[3];
		sfr_data[2] = uip_ethaddr.addr[4];
		sfr_data[3] = uip_ethaddr.addr[5];
		reg_write_m(RTL837X_RLDP_MAGIC0);
		sfr_data[0] = 0;
		sfr_data[1] = 0;
		sfr_data[2] = uip_ethaddr.addr[0];
		sfr_data[3] = uip_ethaddr.addr[1];
		reg_write_m(RTL837X_RLDP_MAGIC1);
		rldp_read_fwd();
	} else {
		for (p = machine.min_port; p <= machine.max_port; p++) {
			rldp_open(p);
			rldp_level[p] = 0;
		}
	}
	rldp_apply();
}


void rldp_port(uint8_t port, __xdata uint8_t on) __banked
{
	if (on) {
		rldp_off_mask &= ~(((uint16_t)1) << port);
	} else {
		rldp_off_mask |= ((uint16_t)1) << port;
		rldp_open(port);
		rldp_level[port] = 0;
	}
	rldp_apply();
}


void rldp_tick(void) __banked
{
	__xdata uint8_t p, pair, st0, st1, looped;

	rldp_read_fwd();
	rldp_apply();

	reg_read_m(RTL837X_RLDP_LOOP_STATE);
	st0 = sfr_data[3];
	st1 = sfr_data[2];

	for (p = machine.min_port; p <= machine.max_port; p++) {
		if (rldp_block[p]) {
			if (!rldp_link(p)) {
				rldp_level[p] = 0;
				rldp_block[p] = 1;
				rldp_open(p);
				continue;
			}
			if (!--rldp_block[p]) {
				rldp_block[p] = 1;
				rldp_open(p);
			}
			continue;
		}
		if (p < 8)
			looped = (st0 >> p) & 1;
		else
			looped = (st1 >> (p - 8)) & 1;
		if (!looped || !((rldp_tx_mask >> p) & 1) || !rldp_link(p))
			continue;

		reg_read_m(RTL837X_RLDP_LOOPPAIR + ((p >> 3) << 2));
		pair = sfr_data[3 - ((p & 7) >> 1)];
		if (p & 1)
			pair >>= 4;
		pair &= 0x0f;
		if (pair > machine.max_port || !((rldp_fwd_mask >> pair) & 1))
			continue;
		if (pair > p && !((rldp_off_mask >> pair) & 1))
			continue;

		rldp_block[p] = RLDP_BLOCK_SECS << rldp_level[p];
		if (rldp_level[p] < RLDP_BLOCK_MAX_SHIFT)
			rldp_level[p]++;
		rldp_mac(p, 0);
		print_string("rldp: loop on port ");
		write_char('0' + machine.log_to_phys_port[p]);
		print_string(", blocked\n");
	}
}


void rldp_show(void) __banked
{
	__xdata uint8_t p, st0, st1, looped;

	print_string(rldp_on ? "rldp on\n" : "rldp off\n");
	reg_read_m(RTL837X_RLDP_LOOP_STATE);
	st0 = sfr_data[3];
	st1 = sfr_data[2];
	for (p = machine.min_port; p <= machine.max_port; p++) {
		print_string("port ");
		write_char('0' + machine.log_to_phys_port[p]);
		print_string(((rldp_off_mask >> p) & 1) ? " off" : " on");
		if (p < 8)
			looped = (st0 >> p) & 1;
		else
			looped = (st1 >> (p - 8)) & 1;
		if (looped && rldp_on && rldp_link(p))
			print_string(" loop");
		if (rldp_block[p]) {
			print_string(" blocked ");
			print_byte(rldp_block[p]);
		}
		write_char('\n');
	}
}

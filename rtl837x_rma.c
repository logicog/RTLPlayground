#include <stdint.h>
#include <stdbool.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_rma.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata uint8_t err_status;
extern __xdata uint8_t cmd_buffer[CMD_BUF_SIZE];
extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[15];
extern __xdata bool stp_enabled;

#define RMA_NONE	0xff

static __code const uint8_t rma_names[RMA_ENTRIES][6] = {
	"00", "01", "02", "03", "04", "08", "0d", "0e", "10", "11",
	"12", "13", "18", "1a", "20", "21", "22", "cdp", "csstp", "lldp"
};
static __code const uint8_t rma_acts[4][8] = {"forward", "trap", "drop", "nocpu"};
static __code const uint8_t rma_flags[4][9] = {"isoleak", "vlanleak", "keep", "storm"};
static __code const uint8_t rma_cares[3][7] = {"delay", "pdelay", "asm"};

static __xdata uint16_t rma_reg;
static __xdata uint8_t  rma_w;
static __xdata uint8_t  rma_i;
static __xdata uint8_t  rma_k;
static __xdata uint8_t  rma_v;
static __xdata uint8_t  rma_p;
static __xdata uint8_t  rma_pass;
static __xdata uint8_t  rma_bit;
static __xdata uint8_t  rma_width;
static __xdata uint8_t  rma_byte;
static __xdata uint8_t  rma_mask;
static __xdata uint8_t  rma_stp;
static __xdata uint8_t  rma_max;

static uint8_t rma_eq(uint8_t w, __code const char *s)
{
	uint8_t i;

	if (w >= cmd_words_len)
		return 0;
	i = cmd_words_b[w];
	while (*s)
		if (cmd_buffer[i++] != *s++)
			return 0;
	i = cmd_buffer[i];
	return i == ' ' || !i;
}

static uint8_t rma_digit(uint8_t w)
{
	uint8_t i;

	if (w >= cmd_words_len)
		return RMA_NONE;
	i = cmd_words_b[w];
	if (cmd_buffer[i + 1] != ' ' && cmd_buffer[i + 1])
		return RMA_NONE;
	i = cmd_buffer[i] - '0';
	return i > 9 ? RMA_NONE : i;
}

static uint8_t rma_port(uint8_t w)
{
	uint8_t p = rma_digit(w);

	if (!p || p > 9)
		return RMA_NONE;
	p = machine.phys_to_log_port[p - 1];
	if (p < machine.min_port || p > machine.max_port)
		return RMA_NONE;
	return p;
}

static void rma_locate(void)
{
	rma_byte = 3 - (rma_bit >> 3);
	rma_mask = ((1 << rma_width) - 1) << (rma_bit & 7);
}

static uint8_t rma_get(void)
{
	rma_locate();
	return (sfr_data[rma_byte] & rma_mask) >> (rma_bit & 7);
}

static void rma_put(void)
{
	rma_locate();
	sfr_data[rma_byte] = (sfr_data[rma_byte] & ~rma_mask) | ((rma_v << (rma_bit & 7)) & rma_mask);
}

static void rma_rmw(void)
{
	if (!rma_pass)
		return;
	reg_read_m(rma_reg);
	rma_put();
	reg_write_m(rma_reg);
}

static uint8_t rma_act(void)
{
	for (rma_k = 0; rma_k < 4; rma_k++)
		if (rma_eq(rma_w, (__code const char *)rma_acts[rma_k]))
			return rma_k;
	return RMA_NONE;
}

static uint8_t rma_onoff(void)
{
	if (rma_eq(rma_w, "on"))
		return 1;
	if (rma_eq(rma_w, "off"))
		return 0;
	return RMA_NONE;
}

static uint8_t rma_num(uint8_t max)
{
	uint8_t n = rma_digit(rma_w);

	return n > max ? RMA_NONE : n;
}

static void rma_print_onoff(__code const char *name)
{
	write_char(' ');
	print_string(name);
	print_string(rma_v ? " on" : " off");
}

static void rma_show(void)
{
	for (rma_i = 0; rma_i < RMA_ENTRIES; rma_i++) {
		reg_read_m(RTL837X_RMA0_CONF + (rma_i << 2));
		print_string("rma ");
		print_string((__code const char *)rma_names[rma_i]);
		write_char(' ');
		rma_bit = RMA_ACT_SHIFT;
		rma_width = 2;
		print_string((__code const char *)rma_acts[rma_get()]);
		rma_width = 1;
		for (rma_k = 4; rma_k--;) {
			rma_bit = rma_k;
			rma_v = rma_get();
			if (rma_k == RMA_FLAG_STORM)
				rma_v = !rma_v;
			rma_print_onoff((__code const char *)rma_flags[rma_k]);
		}
		write_char('\n');
	}
	reg_read_m(RTL837X_RMA_CONF);
	rma_bit = 0;
	rma_width = 3;
	print_string("rma priority ");
	write_char('0' + rma_get());
	rma_bit = 3;
	rma_width = 1;
	print_string("\nrma lldpmatch");
	print_string(rma_get() ? " on\n" : " off\n");
	reg_read_m(RTL837X_RMA_PTP_TRAP_CTRL);
	rma_bit = 0;
	rma_width = 3;
	print_string("rma ptp priority ");
	write_char('0' + rma_get());
	rma_bit = 16;
	rma_width = 2;
	print_string("\nrma ptp cpu ");
	write_char('0' + rma_get());
	write_char('\n');
	for (rma_p = machine.min_port; rma_p <= machine.max_port && rma_p < RMA_PTP_PORTS; rma_p++) {
		print_string("rma ptp ");
		print_phys_port(rma_p);
		rma_bit = rma_p << 1;
		rma_width = 2;
		reg_read_m(RTL837X_RMA_PTP_ETH2_CTRL);
		print_string(" eth2 ");
		print_string((__code const char *)rma_acts[rma_get()]);
		reg_read_m(RTL837X_RMA_PTP_UDP_CTRL);
		print_string(" udp ");
		print_string((__code const char *)rma_acts[rma_get()]);
		rma_bit = rma_p;
		rma_width = 1;
		for (rma_k = 0; rma_k < 3; rma_k++) {
			reg_read_m(RTL837X_RMA_PTP_DELAY_CARE + (rma_k << 2));
			rma_v = rma_get();
			rma_print_onoff((__code const char *)rma_cares[rma_k]);
		}
		write_char('\n');
	}
}

static uint8_t rma_entry(void)
{
	for (rma_pass = 0; rma_pass < 2; rma_pass++) {
		if (rma_pass) {
			rma_reg = RTL837X_RMA0_CONF + (rma_i << 2);
			reg_read_m(rma_reg);
		}
		for (rma_w = 2; rma_w < cmd_words_len;) {
			rma_v = rma_act();
			if (rma_v != RMA_NONE) {
				if (!rma_i && stp_enabled) {
					rma_stp = 1;
					return 0;
				}
				rma_bit = RMA_ACT_SHIFT;
				rma_width = 2;
				rma_put();
				rma_w++;
				continue;
			}
			for (rma_k = 0; rma_k < 4; rma_k++)
				if (rma_eq(rma_w, (__code const char *)rma_flags[rma_k]))
					break;
			if (rma_k == 4)
				return 0;
			rma_w++;
			rma_v = rma_onoff();
			if (rma_v == RMA_NONE)
				return 0;
			if (rma_k == RMA_FLAG_STORM)
				rma_v = !rma_v;
			rma_bit = rma_k;
			rma_width = 1;
			rma_put();
			rma_w++;
		}
		if (rma_pass)
			reg_write_m(rma_reg);
	}
	return 1;
}

static uint8_t rma_ptp_port(void)
{
	rma_p = rma_port(2);
	if (rma_p >= RMA_PTP_PORTS)
		return 0;
	for (rma_pass = 0; rma_pass < 2; rma_pass++) {
		for (rma_w = 3; rma_w < cmd_words_len;) {
			if (rma_eq(rma_w, "eth2") || rma_eq(rma_w, "udp")) {
				rma_reg = rma_eq(rma_w, "eth2") ? RTL837X_RMA_PTP_ETH2_CTRL : RTL837X_RMA_PTP_UDP_CTRL;
				rma_w++;
				if (rma_w >= cmd_words_len)
					return 0;
				rma_v = rma_act();
				if (rma_v == RMA_NONE)
					return 0;
				rma_bit = rma_p << 1;
				rma_width = 2;
				rma_rmw();
				rma_w++;
				continue;
			}
			for (rma_k = 0; rma_k < 3; rma_k++)
				if (rma_eq(rma_w, (__code const char *)rma_cares[rma_k]))
					break;
			if (rma_k == 3)
				return 0;
			rma_reg = RTL837X_RMA_PTP_DELAY_CARE + (rma_k << 2);
			rma_w++;
			rma_v = rma_onoff();
			if (rma_v == RMA_NONE)
				return 0;
			rma_bit = rma_p;
			rma_width = 1;
			rma_rmw();
			rma_w++;
		}
	}
	return cmd_words_len > 3;
}

static uint8_t rma_setting(void)
{
	rma_w = cmd_words_len - 1;
	rma_v = rma_max == 1 ? rma_onoff() : rma_num(rma_max);
	if (rma_v == RMA_NONE)
		return 0;
	rma_pass = 1;
	rma_rmw();
	return 1;
}

void rma_cmd(void) __banked
{
	if (cmd_words_len == 2 && rma_eq(1, "show")) {
		rma_show();
		return;
	}
	if (cmd_words_len == 3 && rma_eq(1, "priority")) {
		rma_reg = RTL837X_RMA_CONF;
		rma_bit = 0;
		rma_width = 3;
		rma_max = 7;
		if (rma_setting())
			return;
	} else if (cmd_words_len == 3 && rma_eq(1, "lldpmatch")) {
		rma_reg = RTL837X_RMA_CONF;
		rma_bit = 3;
		rma_width = 1;
		rma_max = 1;
		if (rma_setting())
			return;
	} else if (cmd_words_len == 4 && rma_eq(1, "ptp") && rma_eq(2, "priority")) {
		rma_reg = RTL837X_RMA_PTP_TRAP_CTRL;
		rma_bit = 0;
		rma_width = 3;
		rma_max = 7;
		if (rma_setting())
			return;
	} else if (cmd_words_len == 4 && rma_eq(1, "ptp") && rma_eq(2, "cpu")) {
		rma_reg = RTL837X_RMA_PTP_TRAP_CTRL;
		rma_bit = 16;
		rma_width = 2;
		rma_max = 3;
		if (rma_setting())
			return;
	} else if (cmd_words_len >= 4 && rma_eq(1, "ptp")) {
		if (rma_ptp_port())
			return;
	} else if (cmd_words_len >= 3) {
		rma_stp = 0;
		for (rma_i = 0; rma_i < RMA_ENTRIES; rma_i++)
			if (rma_eq(1, (__code const char *)rma_names[rma_i]))
				break;
		if (rma_i < RMA_ENTRIES && rma_entry())
			return;
		if (rma_stp) {
			err_status = ERR_INVALID_ARGUMENT;
			print_string("Error: STP sets the action of rma 00 while it runs\n");
			return;
		}
	}
	err_status = ERR_INVALID_ARGUMENT;
	print_string("Error: rma show | <00-22|cdp|csstp|lldp> [forward|trap|drop|nocpu] [storm|keep|vlanleak|isoleak on|off]...\n"
		     "  | priority <0-7> | lldpmatch on|off | ptp priority <0-7> | ptp cpu <0-3>\n"
		     "  | ptp <port> [eth2|udp forward|trap|drop|nocpu] [delay|pdelay|asm on|off]...\n");
}

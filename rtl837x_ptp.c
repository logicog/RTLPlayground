#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_ptp.h"
#include "rtl837x_phy.h"
#include "machine.h"
#include "phy.h"

// cmd_compare and cmd_parse_port_separator use the BANK2-local ABI.
#pragma codeseg BANK2
#pragma constseg BANK2

extern __code const struct machine machine;
extern __xdata uint8_t cmd_buffer[CMD_BUF_SIZE];
extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[15];
extern __xdata uint8_t err_status;
uint8_t cmd_compare(uint8_t word, __code const uint8_t *text);
uint8_t cmd_parse_port_separator(uint8_t index);
extern __xdata uint8_t atoi_results_u8;

static void ptp_error(__code const char *text)
{
	print_string("Error: ");
	print_string(text);
	write_char('\n');
	err_status = ERR_INVALID_ARGUMENT;
}

// Nominal internal-clock setting used by dal_rtl8373_ptp_init().
#define PTP_NOMINAL_FREQ 0x10000000UL
#define PTP_BUSY_TRIES 20

static __xdata uint8_t ptp_sec[6];
static __xdata uint8_t ptp_nsec[4];
static __xdata uint8_t ptp_phy_port;
static __xdata bool ptp_rtl8224;

static uint16_t ptp_read16(uint16_t reg)
{
	if (ptp_rtl8224)
		rtl8224_read_reg_u16(reg);
	else
		reg_read(reg);
	return SFR_DATA_U16;
}

// Preserve the upper half of RTL837x registers and all unselected low bits.
static void ptp_modify16(uint16_t reg, __xdata uint16_t mask, __xdata uint16_t value)
{
	uint16_t v = ptp_read16(reg);
	v = (v & ~mask) | (value & mask);
	if (ptp_rtl8224)
		rtl8224_write_reg_u16(reg, v);
	else {
		SFR_DATA_U16 = v;
		reg_write(reg);
	}
}

static bool ptp_wait(uint16_t reg, __xdata uint16_t mask)
{
	uint8_t tries;
	for (tries = 0; tries < PTP_BUSY_TRIES; tries++) {
		if (!(ptp_read16(reg) & mask))
			return true;
		if (ptp_rtl8224)
			delay(1);
	}
	ptp_error("PTP operation timed out");
	return false;
}

static bool ptp_exec(uint8_t command)
{
	if (!ptp_wait(RTL837X_PTP_TIME_CRTL, PTP_TIME_CMD_EXEC))
		return false;
	ptp_modify16(RTL837X_PTP_TIME_CRTL, 7, command | PTP_TIME_CMD_EXEC);
	return ptp_wait(RTL837X_PTP_TIME_CRTL, PTP_TIME_CMD_EXEC);
}

static void ptp_print_time(void)
{
	uint8_t i;
	for (i = 0; i < 6; i++)
		print_byte(ptp_sec[i]);
	write_char('.');
	for (i = 0; i < 4; i++)
		print_byte(ptp_nsec[i]);
	write_char('\n');
}

static void ptp_read_time(uint16_t sec_base, __xdata uint16_t nsec_base)
{
	uint8_t i;
	uint16_t v;
	for (i = 0; i < 3; i++) {
		v = ptp_read16(sec_base + ((uint16_t)i << 2));
		ptp_sec[4 - (i << 1)] = v >> 8;
		ptp_sec[5 - (i << 1)] = v;
	}
	v = ptp_read16(nsec_base);
	ptp_nsec[2] = v >> 8;
	ptp_nsec[3] = v;
	v = ptp_read16(nsec_base + 4) & 0x3fff;
	ptp_nsec[0] = v >> 8;
	ptp_nsec[1] = v;
}

static void ptp_show(void)
{
	if (!ptp_exec(PTP_TIME_READ))
		return;
	ptp_read_time(RTL837X_PTP_TIME_SEC_RD0, RTL837X_PTP_TIME_NSEC_RD0);
	print_string("Time: ");
	ptp_print_time();
}

// Parse an entire hex token before making any hardware changes.
static bool ptp_hex(uint8_t word, __xdata uint8_t * __xdata dst, __xdata uint8_t size)
{
	uint8_t i, j, c, carry, next;
	__xdata uint8_t n = 0;
	if (word >= cmd_words_len)
		return false;
	for (j = 0; j < size; j++)
		dst[j] = 0;
	i = cmd_words_b[word];
	if (i < CMD_BUF_SIZE - 1 && cmd_buffer[i] == '0'
	    && (cmd_buffer[i + 1] | 0x20) == 'x')
		i += 2;
	while (i < CMD_BUF_SIZE) {
		c = cmd_buffer[i++];
		if (c == ' ' || !c)
			return n != 0;
		if (++n > (uint8_t)(size << 1))
			return false;
		if (c >= '0' && c <= '9')
			c -= '0';
		else if ((c | 0x20) >= 'a' && (c | 0x20) <= 'f')
			c = (c | 0x20) - 'a' + 10;
		else
			return false;
		carry = c;
		j = size;
		while (j) {
			j--;
			next = dst[j] >> 4;
			dst[j] = (dst[j] << 4) | carry;
			carry = next;
		}
	}
	return false;
}

static uint32_t ptp_nsec_value(void)
{
	return ((uint32_t)ptp_nsec[0] << 24) | ((uint32_t)ptp_nsec[1] << 16)
		| ((uint16_t)ptp_nsec[2] << 8) | ptp_nsec[3];
}

static void ptp_set(void)
{
	uint8_t i;
	if (cmd_words_len != 4 || !ptp_hex(2, ptp_sec, 6)
	    || !ptp_hex(3, ptp_nsec, 4) || ptp_nsec_value() >= 1000000000UL) {
		ptp_error(ptp_rtl8224 ? "Usage: ptpphy set <hex:seconds48> <hex:nanoseconds>"
			: "Usage: ptp set <hex:seconds48> <hex:nanoseconds>");
		return;
	}
	if (!ptp_wait(RTL837X_PTP_TIME_CRTL, PTP_TIME_CMD_EXEC))
		return;
	for (i = 0; i < 3; i++)
		ptp_modify16(RTL837X_PTP_TIME_SEC0 + ((uint16_t)i << 2), 0xffff,
			((uint16_t)ptp_sec[4 - (i << 1)] << 8) | ptp_sec[5 - (i << 1)]);
	ptp_modify16(RTL837X_PTP_TIME_NSEC0, 0xffff,
		((uint16_t)ptp_nsec[2] << 8) | ptp_nsec[3]);
	ptp_modify16(RTL837X_PTP_TIME_NSEC1, 0xbfff,
		((uint16_t)ptp_nsec[0] << 8) | ptp_nsec[1] | 0x8000);
	if (ptp_exec(PTP_TIME_WRITE))
		print_string("PTP time loaded\n");
}

static bool ptp_apply_frequency(__xdata uint32_t frequency)
{
	if (!ptp_wait(RTL837X_PTP_APPLY_FREQ, 1))
		return false;
	ptp_modify16(RTL837X_PTP_TIME_FREQ0, 0xffff, frequency);
	ptp_modify16(RTL837X_PTP_TIME_FREQ1, 0xffff, frequency >> 16);
	ptp_modify16(RTL837X_PTP_APPLY_FREQ, 1, 1);
	return ptp_wait(RTL837X_PTP_APPLY_FREQ, 1);
}

static void ptp_init(void)
{
	ptp_modify16(RTL837X_PTP_CLK_SRC_CTRL, 1, 0);
	if (ptp_apply_frequency(PTP_NOMINAL_FREQ))
		print_string("PTP internal clock configured\n");
}

static void ptp_print_frequency(uint16_t base)
{
	uint16_t low;
	print_short(ptp_read16(base + 4));
	low = ptp_read16(base);
	print_byte(low >> 8);
	print_byte(low);
}

static void ptp_frequency(void)
{
	__xdata uint32_t frequency;
	if (cmd_words_len == 3) {
		if (!ptp_hex(2, ptp_nsec, 4) || !(frequency = ptp_nsec_value())) {
			ptp_error(ptp_rtl8224 ? "Usage: ptpphy freq [nonzero hex:adjustment32]"
				: "Usage: ptp freq [nonzero hex:adjustment32]");
			return;
		}
		if (!ptp_apply_frequency(frequency))
			return;
	} else if (cmd_words_len != 2) {
		ptp_error(ptp_rtl8224 ? "Usage: ptpphy freq [nonzero hex:adjustment32]"
			: "Usage: ptp freq [nonzero hex:adjustment32]");
		return;
	}
	print_string("Configured: ");
	ptp_print_frequency(RTL837X_PTP_TIME_FREQ0);
	print_string(" current: ");
	ptp_print_frequency(RTL837X_PTP_CUR_TIME_FREQ0);
	write_char('\n');
}

/* START/STOP are GPI-edge operations, separate from READ/WRITE and APPLY_FREQ.
 * The default is the SDK TIMER_GPIO_OE sequence: enable the output, then
 * clear/set GPO in separate writes, preserving all other bits.
 */
static void ptp_operation(bool start)
{
	__xdata bool self = false;
	if (cmd_words_len == 3) {
		if (cmd_compare(2, "self"))
			self = true;
		else if (!cmd_compare(2, "gpio"))
			goto usage;
	} else if (cmd_words_len != 2) {
		goto usage;
	}
	ptp_modify16(RTL837X_PTP_TIME_OP_CTRL, PTP_OP_MASK,
		start ? PTP_OP_START_RISING : PTP_OP_STOP_RISING);
	if (self)
		ptp_modify16(RTL837X_TOD_UART_SETTING,
			PTP_TIMER_GPIO_OE | PTP_SELF_GPI | PTP_TIMER_GPO, PTP_SELF_GPI);
	else {
		ptp_modify16(RTL837X_TOD_UART_SETTING, PTP_TIMER_GPIO_OE, PTP_TIMER_GPIO_OE);
		ptp_modify16(RTL837X_TOD_UART_SETTING, PTP_TIMER_GPO, 0);
	}
	ptp_modify16(RTL837X_TOD_UART_SETTING, PTP_TIMER_GPO, PTP_TIMER_GPO);
	print_string("PTP trigger sent\n");
	return;
usage:
	ptp_error("Usage: ptp start|stop [self|gpio]");
}

// RTL837x MAC3..8 correspond to PTP4,0,1,2,3,5.
static uint8_t ptp_slot(uint8_t port)
{
	if (port == 3)
		return 4;
	if (port == 8)
		return 5;
	return port - 4;
}

static void ptp_port(__xdata bool trap)
{
	uint8_t port, action_bit, role = 1;
	uint16_t reg;
	__xdata bool enable;
	if (cmd_words_len != 4)
		goto usage;
	if (cmd_compare(3, "off"))
		role = 0;
	else if (!trap && cmd_compare(3, "e2e"))
		role = 2;
	else if (!trap && cmd_compare(3, "p2p"))
		role = 3;
	else if (!cmd_compare(3, "on"))
		goto usage;
	enable = role != 0;
	if (!cmd_parse_port_separator(cmd_words_b[2]))
		goto usage;
	port = atoi_results_u8;
	if (!MACHINE_PORT_VALID(port) || port < 3 || port > 8) {
		ptp_error("This port has no supported RTL837x PTP slot");
		return;
	}
	reg = RTL837X_P0_MISC_CTRL + ((uint16_t)ptp_slot(port) << 5);
	if (!trap) {
		// ETH/UDP parsing, port role, and TX timestamp capture; keep link delay.
		ptp_modify16(reg - 8, 0x002f, enable ? ((uint16_t)role << 2) | 0x0023 : 0);
		ptp_modify16(reg, 1, enable ? 0 : 1);
		if (enable)
			ptp_modify16(reg + (RTL837X_P0_PTP_PORTID - RTL837X_P0_MISC_CTRL), 0x3f, port);
	} else {
		// One shared register per category. CARE has one bit per MAC port.
		action_bit = port << 1;
		// SDK trap enable uses action01, as does RMAOP_TRAP_TO_CPU.
		reg_bit_clear(RTL837X_RMA_PORT_PTP_ETH2_CTRL, action_bit + 1);
		reg_bit_clear(RTL837X_RMA_PORT_PTP_UDP_CTRL, action_bit + 1);
		if (enable) {
			reg_bit_set(RTL837X_RMA_PORT_PTP_ETH2_CTRL, action_bit);
			reg_bit_set(RTL837X_RMA_PORT_PTP_UDP_CTRL, action_bit);
			reg_bit_set(RTL837X_RMA_PORT_PTP_DELAY_CARE_CTRL, port);
			reg_bit_set(RTL837X_RMA_PORT_PTP_PDELAY_CARE_CTRL, port);
			reg_bit_set(RTL837X_RMA_PORT_PTP_ASM_CARE_CTRL, port);
		} else {
			reg_bit_clear(RTL837X_RMA_PORT_PTP_ETH2_CTRL, action_bit);
			reg_bit_clear(RTL837X_RMA_PORT_PTP_UDP_CTRL, action_bit);
			reg_bit_clear(RTL837X_RMA_PORT_PTP_DELAY_CARE_CTRL, port);
			reg_bit_clear(RTL837X_RMA_PORT_PTP_PDELAY_CARE_CTRL, port);
			reg_bit_clear(RTL837X_RMA_PORT_PTP_ASM_CARE_CTRL, port);
		}
	}
	print_string("PTP port updated\n");
	return;
usage:
	ptp_error("Usage: ptp port <port> on|off|e2e|p2p; trap <port> on|off");
}

static void ptp_tx(void)
{
	uint16_t v, sequence;
	uint8_t port, type, sec;
	v = ptp_read16(RTL837X_PTP_TX_TIMESTAMP_RD0);
	if (!(v & 0x8000)) {
		print_string("PTP TX FIFO empty\n");
		return;
	}
	port = (v >> 8) & 0x3f;
	type = (v >> 6) & 3;
	sequence = (v & 0x3f) << 10;
	v = ptp_read16(RTL837X_PTP_TX_TIMESTAMP_RD1);
	sequence |= v >> 6;
	sec = (v & 0x3f) << 2;
	v = ptp_read16(RTL837X_PTP_TX_TIMESTAMP_RD2);
	sec |= v >> 14;
	v &= 0x3fff;
	ptp_nsec[0] = v >> 8;
	ptp_nsec[1] = v;
	v = ptp_read16(RTL837X_PTP_TX_TIMESTAMP_RD3);
	ptp_nsec[2] = v >> 8;
	ptp_nsec[3] = v;
	print_string("TX MAC port: "); print_byte(port);
	print_string(" type: "); print_byte(type);
	print_string(" seq: "); print_short(sequence);
	print_string(" sec8: "); print_byte(sec);
	write_char('.');
	for (port = 0; port < 4; port++)
		print_byte(ptp_nsec[port]);
	write_char('\n');
}

static void ptp_clkout(void)
{
	ptp_read_time(RTL837X_PTP_CLKOUT_SEC0, RTL837X_PTP_CLKOUT_NSEC0);
	print_string("Clock output start configuration: ");
	ptp_print_time();
	print_string("Clock output control: ");
	print_short(ptp_read16(RTL837X_PTP_CLKOUT_CTRL));
	write_char('\n');
}

void ptp_rtl8224_init(void) __banked
{
	uint8_t port;
	if (!machine.isRTL8373) {
		ptp_error("This board has no RTL8224");
		return;
	}
	ptp_rtl8224 = true;
	if (ptp_wait(RTL837X_PTP_APPLY_FREQ, 1)) {
		for (port = 0; port < 4; port++)
			ptp_modify16(RTL837X_P0_MISC_CTRL + ((uint16_t)port << 5), 1, 0);
		ptp_modify16(RTL837X_PTP_CLK_SRC_CTRL, 1, 0);
		if (ptp_apply_frequency(PTP_NOMINAL_FREQ)) {
			ptp_modify16(RTL837X_PTP_TIME_OP_CTRL, PTP_OP_MASK, PTP_OP_START_RISING);
			print_string("RTL8224 internal clock configured\n");
		}
	}
	ptp_rtl8224 = false;
}

void ptp_rtl8224_get_time(void) __banked
{
	if (!machine.isRTL8373) {
		ptp_error("This board has no RTL8224");
		return;
	}
	ptp_rtl8224 = true;
	ptp_show();
	ptp_rtl8224 = false;
}

void ptp_rtl8224_set_time(void) __banked
{
	if (!machine.isRTL8373) {
		ptp_error("This board has no RTL8224");
		return;
	}
	ptp_rtl8224 = true;
	ptp_set();
	ptp_rtl8224 = false;
}

static void rtl8224_ptp_parse(void)
{
	ptp_rtl8224 = true;
	if (cmd_words_len == 1 || (cmd_words_len == 2 && cmd_compare(1, "init")))
		ptp_rtl8224_init();
	else if (cmd_words_len == 2 && (cmd_compare(1, "show") || cmd_compare(1, "cur")))
		ptp_rtl8224_get_time();
	else if (cmd_compare(1, "set"))
		ptp_rtl8224_set_time();
	else if (cmd_compare(1, "freq"))
		ptp_frequency();
	else if (cmd_words_len == 2 && cmd_compare(1, "tx"))
		ptp_tx();
	else if (cmd_words_len == 2 && cmd_compare(1, "clkout"))
		ptp_clkout();
	else
		ptp_error("Usage: ptpphy [init|show|cur|set <sec48> <ns>|freq [raw32]|tx|clkout]");
	ptp_rtl8224 = false;
}

// RTL8221 uses MMD31 commands.
static uint16_t rtl8221_ptp_read(uint16_t reg)
{
	phy_read(ptp_phy_port, PHY_MMD31, reg);
	return SFR_DATA_U16;
}

static void rtl8221_ptp_modify(uint16_t reg, __xdata uint16_t mask, __xdata uint16_t value)
{
	uint16_t v = rtl8221_ptp_read(reg);
	phy_write(ptp_phy_port, PHY_MMD31, reg, (v & ~mask) | (value & mask));
}

static bool rtl8221_ptp_wait(uint8_t mmd, __xdata uint16_t reg, __xdata uint16_t mask)
{
	uint8_t tries;
	for (tries = 0; tries < PTP_BUSY_TRIES; tries++) {
		phy_read(ptp_phy_port, mmd, reg);
		if (!(SFR_DATA_U16 & mask))
			return true;
		delay(1);
	}
	ptp_error("RTL8221 PTP operation timed out");
	return false;
}

static bool rtl8221_ptp_exec(__xdata uint8_t mode)
{
	if (!rtl8221_ptp_wait(PHY_MMD31, RTL8221_PTP_CLK_CFG, RTL8221_PTP_CLK_EXEC))
		return false;
	rtl8221_ptp_modify(RTL8221_PTP_CLK_CFG, 0x000f, ((uint16_t)mode << 1) | RTL8221_PTP_CLK_EXEC);
	return rtl8221_ptp_wait(PHY_MMD31, RTL8221_PTP_CLK_CFG, RTL8221_PTP_CLK_EXEC);
}

// A snapshot has 48-bit seconds and 30-bit nanoseconds, at two-byte strides.
static void rtl8221_ptp_time(uint16_t sec_base, __xdata uint16_t nsec_base)
{
	uint8_t i;
	uint16_t v;
	for (i = 0; i < 3; i++) {
		v = rtl8221_ptp_read(sec_base + ((uint16_t)i << 1));
		ptp_sec[4 - (i << 1)] = v >> 8;
		ptp_sec[5 - (i << 1)] = v;
	}
	v = rtl8221_ptp_read(nsec_base);
	ptp_nsec[2] = v >> 8; ptp_nsec[3] = v;
	v = rtl8221_ptp_read(nsec_base + 2) & 0x3fff;
	ptp_nsec[0] = v >> 8; ptp_nsec[1] = v;
	ptp_print_time();
}

static void rtl8221_ptp_init(void)
{
	__xdata bool reset;
	if (!rtl8221_ptp_wait(PHY_MMD_AN, PHY_ANEG_CTRL, 0x8000)
	    || !rtl8221_ptp_wait(PHY_MMD31, RTL8221_PTP_CLK_CFG, RTL8221_PTP_CLK_EXEC))
		return;
	reset = (rtl8221_ptp_read(RTL8221_PTP_CLK_CFG) & RTL8221_PTP_CLK_INPUT_MASK) != RTL8221_PTP_CLK_FREE_RUNNING;
	reset |= !(rtl8221_ptp_read(RTL8221_PTP_CTRL) & RTL8221_PTP_ENABLE);
	// Preserve packet formats, timestamp insertion, DLL and link/EEE settings.
	rtl8221_ptp_modify(RTL8221_PTP_INTERNAL_CTRL, RTL8221_PTP_INTERNAL_MASK, RTL8221_PTP_INTERNAL_25M);
	rtl8221_ptp_modify(RTL8221_PTP_CTRL, RTL8221_PTP_ENABLE, RTL8221_PTP_ENABLE);
	// CLKIN tracking overrides software rate adjustments.
	rtl8221_ptp_modify(RTL8221_PTP_CLK_CFG, RTL8221_PTP_CLK_INPUT_MASK, RTL8221_PTP_CLK_FREE_RUNNING);
	if (reset) {
		// PTP_Enable/CLKIN changes require software reset.
		print_string("Applying RTL8221 PTP settings resets this PHY link\n");
		phy_read(ptp_phy_port, PHY_MMD_AN, PHY_ANEG_CTRL);
		phy_write(ptp_phy_port, PHY_MMD_AN, PHY_ANEG_CTRL, SFR_DATA_U16 | 0x8000);
		if (!rtl8221_ptp_wait(PHY_MMD_AN, PHY_ANEG_CTRL, 0x8000))
			return;
	}
	if ((rtl8221_ptp_read(RTL8221_PTP_INTERNAL_CTRL) & RTL8221_PTP_INTERNAL_MASK) != RTL8221_PTP_INTERNAL_25M
	    || !(rtl8221_ptp_read(RTL8221_PTP_CTRL) & RTL8221_PTP_ENABLE)
	    || (rtl8221_ptp_read(RTL8221_PTP_CLK_CFG) & RTL8221_PTP_CLK_INPUT_MASK) != RTL8221_PTP_CLK_FREE_RUNNING) {
		ptp_error("RTL8221 did not retain PTP clock settings");
		return;
	}
	print_string("RTL8221 internal clock configured\n");
}

static void rtl8221_ptp_set(void)
{
	uint8_t i;
	if (!rtl8221_ptp_wait(PHY_MMD31, RTL8221_PTP_CLK_CFG, RTL8221_PTP_CLK_EXEC))
		return;
	for (i = 0; i < 3; i++)
		phy_write(ptp_phy_port, PHY_MMD31, RTL8221_PTP_S_LO + ((uint16_t)i << 1),
			((uint16_t)ptp_sec[4 - (i << 1)] << 8) | ptp_sec[5 - (i << 1)]);
	phy_write(ptp_phy_port, PHY_MMD31, RTL8221_PTP_NS_LO, ((uint16_t)ptp_nsec[2] << 8) | ptp_nsec[3]);
	rtl8221_ptp_modify(RTL8221_PTP_NS_HI, 0x3fff, ((uint16_t)ptp_nsec[0] << 8) | ptp_nsec[1]);
	if (rtl8221_ptp_exec(RTL8221_PTP_CLK_WRITE))
		print_string("RTL8221 PTP time loaded\n");
}

static void rtl8221_ptp_frequency(void)
{
	if (cmd_words_len == 4) {
		if (!rtl8221_ptp_wait(PHY_MMD31, RTL8221_PTP_CLK_CFG, RTL8221_PTP_CLK_EXEC))
			return;
		phy_write(ptp_phy_port, PHY_MMD31, RTL8221_PTP_NS_LO, ((uint16_t)ptp_nsec[2] << 8) | ptp_nsec[3]);
		rtl8221_ptp_modify(RTL8221_PTP_NS_HI, 0x003f, ((uint16_t)ptp_nsec[0] << 8) | ptp_nsec[1]);
		if (!rtl8221_ptp_exec(RTL8221_PTP_RATE_WRITE))
			return;
	}
	if (!rtl8221_ptp_exec(RTL8221_PTP_RATE_READ))
		return;
	print_string("RTL8221 raw rate adjustment22: ");
	print_short(rtl8221_ptp_read(RTL8221_PTP_NS_HI) & 0x3f);
	print_byte(rtl8221_ptp_read(RTL8221_PTP_NS_LO) >> 8);
	print_byte(SFR_DATA_U16);
	write_char('\n');
}

static void rtl8221_ptp_capture(void)
{
	__xdata bool enable = cmd_compare(3, "on") != 0;
	if (enable && !(rtl8221_ptp_read(RTL8221_PTP_CTRL) & RTL8221_PTP_ENABLE)) {
		ptp_error("Initialize RTL8221 PTP first");
		return;
	}
	// Enable PTPv2 L2/IPv4/IPv6 recognition without timestamp insertion.
	rtl8221_ptp_modify(RTL8221_PTP_CTRL, RTL8221_PTP_CAPTURE_MASK, enable ? RTL8221_PTP_CAPTURE_MASK : 0);
	print_string(enable ? "RTL8221 PTP capture enabled\n" : "RTL8221 PTP capture disabled\n");
}

static void rtl8221_ptp_timestamp(__xdata bool rx)
{
	__xdata uint8_t type = 0;
	__xdata uint16_t status;
	if (cmd_words_len == 4)
		type = cmd_buffer[cmd_words_b[3]] - '0';
	if (!rtl8221_ptp_wait(PHY_MMD31, RTL8221_PTP_TS_STATUS, RTL8221_PTP_TS_READ))
		return;
	status = rtl8221_ptp_read(RTL8221_PTP_TS_STATUS);
	if (!(status & ((uint16_t)1 << ((rx ? 11 : 15) - type)))) {
		print_string("RTL8221 timestamp not ready\n");
		return;
	}
	// Upper bits are read-only readiness flags.
	phy_write(ptp_phy_port, PHY_MMD31, RTL8221_PTP_TS_STATUS,
		(status & 0x10) | ((uint16_t)type << 2) | (rx ? 2 : 0) | RTL8221_PTP_TS_READ);
	if (!rtl8221_ptp_wait(PHY_MMD31, RTL8221_PTP_TS_STATUS, RTL8221_PTP_TS_READ))
		return;
	print_string(rx ? "RX info: " : "TX info: ");
	print_short(rtl8221_ptp_read(RTL8221_PTP_TS_INFO));
	print_string(" source hash: "); print_short(rtl8221_ptp_read(RTL8221_PTP_TS_HASH));
	print_string(" seq: "); print_short(rtl8221_ptp_read(RTL8221_PTP_TS_SEQ));
	print_string(" time: ");
	rtl8221_ptp_time(RTL8221_PTP_TS_S_LO, RTL8221_PTP_TS_NS_LO);
}

static void rtl8221_ptp_parse(void)
{
	uint16_t id;
	// Validate syntax and values before accessing any PHY.
	if (cmd_words_len < 3)
		goto usage;
	if (cmd_compare(2, "set")) {
		if (cmd_words_len != 5 || !ptp_hex(3, ptp_sec, 6)
		    || !ptp_hex(4, ptp_nsec, 4) || ptp_nsec_value() >= 1000000000UL)
			goto usage;
	} else if (cmd_compare(2, "freq")) {
		if (cmd_words_len != 3 && (cmd_words_len != 4
		    || !ptp_hex(3, ptp_nsec, 4) || ptp_nsec_value() > 0x3fffffUL))
			goto usage;
	} else if (cmd_compare(2, "capture")) {
		if (cmd_words_len != 4 || (!cmd_compare(3, "on") && !cmd_compare(3, "off")))
			goto usage;
	} else if (cmd_compare(2, "tx") || cmd_compare(2, "rx")) {
		if (cmd_words_len != 3 && (cmd_words_len != 4
		    || cmd_buffer[cmd_words_b[3]] < '0' || cmd_buffer[cmd_words_b[3]] > '3'
		    || (cmd_buffer[cmd_words_b[3] + 1] && cmd_buffer[cmd_words_b[3] + 1] != ' ')))
			goto usage;
	} else if (cmd_words_len != 3 || (!cmd_compare(2, "init")
	    && !cmd_compare(2, "show") && !cmd_compare(2, "cur"))) {
		goto usage;
	}
	if (!cmd_parse_port_separator(cmd_words_b[1]))
		goto usage;
	ptp_phy_port = atoi_results_u8;
	// RTL8372 external copper PHYs occupy MAC3/8; exclude SFP and 10G PHYs.
	if (machine.isRTL8373 || !MACHINE_PORT_VALID(ptp_phy_port) || machine.is_sfp[ptp_phy_port]
	    || !((ptp_phy_port == 3 && !machine.n_10g) || (ptp_phy_port == 8 && machine.n_10g < 2))) {
		ptp_error("This port has no supported RTL8221");
		return;
	}
	phy_read(ptp_phy_port, PHY_MMD_PMAPMD, 2);
	id = SFR_DATA_U16;
	phy_read(ptp_phy_port, PHY_MMD_PMAPMD, 3);
	if (id != 0x001c || (SFR_DATA_U16 & 0xfff0) != 0xc840) {
		ptp_error("RTL8221 PHY identifier not found");
		return;
	}
	if (cmd_compare(2, "init"))
		rtl8221_ptp_init();
	else if (cmd_compare(2, "show") || cmd_compare(2, "cur")) {
		if (rtl8221_ptp_exec(RTL8221_PTP_CLK_READ)) {
			print_string("Time: ");
			rtl8221_ptp_time(RTL8221_PTP_S_LO, RTL8221_PTP_NS_LO);
		}
	} else if (cmd_compare(2, "set"))
		rtl8221_ptp_set();
	else if (cmd_compare(2, "freq"))
		rtl8221_ptp_frequency();
	else if (cmd_compare(2, "capture"))
		rtl8221_ptp_capture();
	else
		rtl8221_ptp_timestamp(cmd_compare(2, "rx") != 0);
	return;
usage:
	ptp_error("Usage: ptpphy <port> init|show|cur|set <sec48> <ns>|freq [raw22]|capture on|off|tx|rx [0..3]");
}

void ptp_parse(bool phy) __banked
{
	ptp_rtl8224 = false;
	// A typo must not initialize the clock.
	if (phy) {
		if (machine.isRTL8373)
			rtl8224_ptp_parse();
		else
			rtl8221_ptp_parse();
		return;
	}
	if (cmd_words_len == 1 || (cmd_words_len == 2 && cmd_compare(1, "init")))
		ptp_init();
	else if (cmd_words_len == 2 && (cmd_compare(1, "show") || cmd_compare(1, "cur")))
		ptp_show();
	else if (cmd_compare(1, "set"))
		ptp_set();
	else if (cmd_compare(1, "start") || cmd_compare(1, "stop"))
		ptp_operation(cmd_compare(1, "start") != 0);
	else if (cmd_compare(1, "freq"))
		ptp_frequency();
	else if (cmd_compare(1, "port") || cmd_compare(1, "trap"))
		ptp_port(cmd_compare(1, "trap") != 0);
	else if (cmd_words_len == 2 && cmd_compare(1, "tx"))
		ptp_tx();
	else if (cmd_words_len == 2 && cmd_compare(1, "clkout"))
		ptp_clkout();
	else
		ptp_error("Usage: ptp [init|show|cur|set|start|stop|freq|port|trap|tx|clkout]");
}

#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "machine.h"
#include "snmp_hw.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code const struct machine machine;
static __xdata uint8_t fault;

void snmp_hw_init(void) __banked
{
	/* Establish quarantine state explicitly, independent of the CRT. */
	fault = 0;
}

/* The host edge models SFR command completion, including a stuck engine. */
#ifdef RTLP_HOST_TEST
extern void snmp_hw_command(uint16_t reg, uint8_t command);
#endif
static bool access(uint16_t reg, uint8_t command)
{
	uint8_t budget = 64;
	if (fault)
		return false;
	/* Never replace a command still in flight. */
	if (SFR_EXEC_STATUS) {
		fault = 1;
		return false;
	}
	SFR_REG_ADDR_U16 = reg;
	SFR_EXEC_GO = command;
#ifdef RTLP_HOST_TEST
	snmp_hw_command(reg, command);
#endif
	do {
		if (!SFR_EXEC_STATUS)
			return true;
	} while (--budget);
	fault = 1;
	return false;
}

static void copy_word(__xdata uint8_t *__xdata value)
{
	value[0] = SFR_DATA_24;
	value[1] = SFR_DATA_16;
	value[2] = SFR_DATA_8;
	value[3] = SFR_DATA_0;
}

static bool user_port(uint8_t port)
{
	return port < CPU_PORT && port >= machine.min_port && port <= machine.max_port;
}

bool snmp_hw_counter(uint8_t port, __xdata uint8_t selector,
		     __xdata struct snmp_counter *__xdata value) __banked
{
	uint8_t budget = 32;
	/* Only the eight native 64-bit interface counters are defined here. */
	if (!user_port(port) || selector > 7 || fault)
		return false;
	if (SFR_EXEC_STATUS) {
		fault = 1;
		return false;
	}
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = selector >> 3;
	SFR_DATA_0 = (selector << 5) | (port << 1) | 1;
	if (!access(RTL837X_STAT_GET, SFR_EXEC_WRITE_REG))
		return false;
	do {
		if (!access(RTL837X_STAT_GET, SFR_EXEC_READ_REG))
			return false;
		if (!(SFR_DATA_0 & 1)) {
			if (!access(RTL837X_STAT_V_HIGH, SFR_EXEC_READ_REG))
				return false;
			copy_word(value->bytes);
			if (!access(RTL837X_STAT_V_LOW, SFR_EXEC_READ_REG))
				return false;
			copy_word(value->bytes + 4);
			return true;
		}
	} while (--budget);
	fault = 1;
	return false;
}

bool snmp_hw_link(uint8_t port, __xdata uint32_t *__xdata value) __banked
{
	if (!user_port(port) || !access(RTL837X_REG_LINKS_STS, SFR_EXEC_READ_REG))
		return false;
	/* GUI: sfr_data[(port / 8) + 1], i.e. A5 for MAC0..7, A6 for MAC8.
	 * A4/A7 are not the user-port bitmap; do not expose their bits as link. */
	*value = SFR_DATA_16 | ((uint16_t)SFR_DATA_8 << 8);
	return true;
}

/* Return zero when no negotiated speed is available; never infer a capability. */
bool snmp_hw_speed(uint8_t port, __xdata uint16_t *__xdata mbps) __banked
{
	__xdata uint32_t links;
	__xdata uint8_t code;
	if (!snmp_hw_link(port, &links))
		return false;
	*mbps = 0;
	/* snmp_hw_link() has already validated the MAC port range 0..8. */
	if (!(links & ((uint16_t)1 << port)))
		return true;
	if (!access(port < 8 ? RTL837X_REG_LINKS : RTL837X_REG_LINKS_89, SFR_EXEC_READ_REG))
		return false;
	switch ((port & 7) >> 1) {
	case 0:
		code = SFR_DATA_0;
		break;
	case 1:
		code = SFR_DATA_8;
		break;
	case 2:
		code = SFR_DATA_16;
		break;
	default:
		code = SFR_DATA_24;
		break;
	}
	code = (port & 1) ? code >> 4 : code & 15;
	switch (code) {
	case 0:
		*mbps = 10;
		break;
	case 1:
		*mbps = 100;
		break;
	case 2:
		*mbps = 1000;
		break;
	case 4:
		*mbps = 10000;
		break;
	case 5:
		*mbps = 2500;
		break;
	case 6:
		*mbps = 5000;
		break;
	default:
		break;
	}
	return true;
}

/*
 * test_eee_8370.c - the EEE round trip on the RTL836x (RTL8370 / RTL8367B):
 * what port_eee_enable()/port_eee_disable() program -- the MAC enable word at
 * RTL836x_PORT_EEE_CFG_REG(p) plus the PHY page 5/7 sequence through the
 * 0x2000|(p<<5)|reg window -- and what port_eee_status() decodes back out of
 * the same register.
 *
 * The die quirk this pins down: port_eee_enable() takes a speed argument like
 * the RTL8372/3 version but IGNORES it here. The MAC enable turns 100M and 1G
 * on together in one word (0x0f00), there is no per-speed advertisement
 * register, and the PHY writes are the fixed page sequence of the EEE-capable
 * PHYs. The CLI now shares the RTL8372/3 parsing (parse_eee() has no
 * RTL836X branch any more), so `eee on <port> 2g5` is accepted and simply
 * means "EEE on" on this die -- which is why the parse_eee() comment and the
 * port_eee_enable() near-no-op on `speed` belong to the same change.
 *
 * This is one of the host binaries built with -DMACHINE_NAME=SODOLA_SL_SG008W,
 * so machine.h derives SOC_FAMILY=RTL836X and the RTL836x branches are taken.
 *
 * Register-view convention (the one test_l2_mgmt_8370.c documents): REG_SET
 * stages a 16-bit value in the register's HIGH half (SFR_WRITE_U16 puts the
 * bytes in sfr_data[0..1]), while the reg_read_m() + rtl836x_sfr_u16() view --
 * which is what port_eee_status() and rtl836x_eee_mac_set()'s read-modify-
 * write go through -- is the LOW half (sfr_data[2..3], and reg_write_m() packs
 * it back there). So the MAC enable word is written through reg_write_m() and
 * reads out of the low half, while the PHY registers go out through REG_SET
 * and read out of the high half.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtlos_regs.h"
#include "rtl837x_port.h"
#include "machine.h"
#include "support.h"
#include "hw_mock.h"
#include "page_impl.h"

extern uint8_t  outbuf[TCP_OUTBUF_SIZE];
extern uint16_t slen;

void delay(uint16_t ms) { (void)ms; }

/* The response body send_eee() builds in outbuf, after the HTTP header. */
static const char *body(void)
{
	outbuf[slen < TCP_OUTBUF_SIZE ? slen : TCP_OUTBUF_SIZE - 1] = 0;
	const char *p = strstr((const char *)outbuf, "\r\n\r\n");
	return p ? p + 4 : (const char *)outbuf;
}

static int count(const char *needle)
{
	int n = 0;
	for (const char *p = body(); (p = strstr(p, needle)); p += strlen(needle))
		n++;
	return n;
}

/* Brackets and braces balance outside strings, quotes pair up. */
static int balanced(void)
{
	int depth = 0, instr = 0;
	for (const char *p = body(); *p; p++) {
		if (*p == '"') { instr = !instr; continue; }
		if (instr) continue;
		if (*p == '{' || *p == '[') depth++;
		if (*p == '}' || *p == ']') depth--;
		if (depth < 0) return 0;
	}
	return depth == 0 && !instr;
}

#define EEE_ON (RTL836x_EEE_TX_BIT | RTL836x_EEE_RX_BIT | \
		RTL836x_EEE_GIGA_BIT | RTL836x_EEE_100M_BIT)

/* PHY registers go out through REG_SET, i.e. in the register's high half. */
static uint16_t phy_reg(uint8_t port, uint8_t reg)
{
	return (uint16_t)(hw_reg_get((uint16_t)(RTL836x_PHY_BASE +
			     ((uint16_t)port << 5) + reg)) >> 16);
}

/* The MAC enable word is written with reg_write_m(), which stores
 * sfr_data[2..3] in the regfile's low half -- where reg_read_m() +
 * rtl836x_sfr_u16() read it back from. */
static uint16_t mac_cfg(uint8_t port)
{
	return (uint16_t)hw_reg_get(RTL836x_PORT_EEE_CFG_REG(port));
}

static void t_enable_disable(void)
{
	printf("[test] eee on / eee off for one port round trip\n");
	hw_reset();
	out_reset();

	port_eee_enable(3, EEE_1000);

	CHECK((mac_cfg(3) & EEE_ON) == EEE_ON,
	      "the MAC enable word carries tx, rx, 100M and 1G together (no speed word field)");
	CHECK(strstr(out_buf, "EEE on for") != NULL, "and the console reports it");
	CHECK(strstr(out_buf, "100m\n") == NULL, "no RTL8372/3 advertisement line (no per-speed decode exists here)");
	CHECK(phy_reg(3, 6) == RTL836x_EEE_PHY_REG6_ENABLE,
	      "page 5 reg 6 enables the PHY's EEE capability");
	CHECK(phy_reg(3, RTL836x_EEE_PHY_REG21) == RTL836x_EEE_PHY_ENABLE_REG21,
	      "page 7 reg 21 holds the EEE enable value");
	CHECK(phy_reg(3, RTL836x_EEE_PHY_REG30) == RTL836x_EEE_PHY_REG30_VALUE,
	      "page 7 reg 30 holds its fixed value");
	CHECK(phy_reg(3, 31) == RTL836x_EEE_PHY_REG31_PAGE0,
	      "and the PHY is left on page 0");

	port_eee_disable(3);

	CHECK((mac_cfg(3) & EEE_ON) == 0, "the MAC enable bits are cleared again");
	CHECK(phy_reg(3, 6) == RTL836x_EEE_PHY_REG6_DISABLE,
	      "page 5 reg 6 disables the capability again");
	CHECK(phy_reg(3, RTL836x_EEE_PHY_REG21) == RTL836x_EEE_PHY_DISABLE_REG21,
	      "page 7 reg 21 holds the disable value");
}

static void t_status_decodes(void)
{
	printf("[test] the status decoder reads the bits back out of the register\n");
	hw_reset();
	out_reset();

	port_eee_enable(4, EEE_1000);
	port_eee_status(4);

	CHECK(strstr(out_buf, "Port: ") != NULL, "one line per port");
	CHECK(strstr(out_buf, "100M") != NULL, "the 100M bit decodes");
	CHECK(strstr(out_buf, "1G") != NULL, "so does the giga bit");
	CHECK(strstr(out_buf, " rx") != NULL && strstr(out_buf, " tx") != NULL,
	      "rx and tx enable bits decode");
	CHECK(strstr(out_buf, "LPI idle") != NULL,
	      "no LPI bit means the link is not in LPI");

	/* Turn LPI on through the firmware's own access path (read, set bit
	 * 0x0020 in sfr_data[3], write back) and decode again. */
	reg_read_m(RTL836x_PORT_EEE_CFG_REG(4));
	sfr_data[3] |= (uint8_t)(RTL836x_EEE_LPI_BIT & 0xff);
	reg_write_m(RTL836x_PORT_EEE_CFG_REG(4));
	out_reset();
	port_eee_status(4);

	CHECK(strstr(out_buf, "LPI active") != NULL,
	      "and the LPI bit decodes as active");
}

static void t_enable_all(void)
{
	printf("[test] eee on for all ports (the boot-time arm)\n");
	hw_reset();
	out_reset();

	/* The same call rtl8370_init() makes now; the speed argument is the
	 * one the RTL8372/3 init paths pass down the family convention, and
	 * this die's port_eee_enable() ignores it. */
	port_eee_enable_all(EEE_1000 | EEE_NORESET);

	for (uint8_t p = MACHINE_MIN_PORT; p <= MACHINE_MAX_PORT; p++) {
		CHECK((mac_cfg(p) & EEE_ON) == EEE_ON, "every port's MAC enable word is set");
		CHECK(phy_reg(p, 6) == RTL836x_EEE_PHY_REG6_ENABLE,
		      "every port's PHY got the page sequence");
	}
	CHECK(strstr(out_buf, "EEE on for") != NULL, "and the console reports each port");
}

static void t_web_json(void)
{
	printf("[test] /eee.json: the partner column is not a copy of the local flags\n");
	hw_reset();
	send_eee();

	CHECK(balanced(), "valid JSON: quotes pair up, brackets balance");
	CHECK(count("\"portNum\":") == MACHINE_MAX_PORT - MACHINE_MIN_PORT + 1,
	      "one object per port");
	CHECK(count("\"eee\":\"000\"") == MACHINE_MAX_PORT - MACHINE_MIN_PORT + 1,
	      "all-zero registers report no local EEE advertisement");
	CHECK(count("\"eee_lp\":\"000\"") == MACHINE_MAX_PORT - MACHINE_MIN_PORT + 1,
	      "and no link-partner ability either (the web tab shows '-')");

	/* Enable port 3 through the firmware's own path and send again: the
	 * local flags turn to 011 (1G + 100M, the die's fixed enable), while
	 * eee_lp must stay 000 -- it used to echo the local bits back, which
	 * made the web tab claim the link partner advertised what we enable. */
	hw_reset();
	port_eee_enable(3, EEE_1000);
	send_eee();

	CHECK(balanced(), "still valid JSON after the enable");
	CHECK(count("\"eee\":\"011\"") == 1,
	      "exactly one port now advertises EEE locally");
	CHECK(count("\"eee_lp\":\"000\"") == MACHINE_MAX_PORT - MACHINE_MIN_PORT + 1,
	      "every partner column is unknown, not a copy of the local flags");
	CHECK(strstr(body(), "\"eee\":\"011\",\"eee_lp\":\"000\",\"active\":") != NULL,
	      "the enabled port's object ties the three fields together");
}

int main(void)
{
	printf("== rtl837x_port.c RTL836x EEE tests ==\n");
	t_enable_disable();
	t_status_decodes();
	t_enable_all();
	t_web_json();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

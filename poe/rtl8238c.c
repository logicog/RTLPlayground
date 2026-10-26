/*
 * RTL8238C PSE driver.
 *
 * The PSE has no firmware in its own flash: the vendor switch firmware keeps
 * the image in its flash (bank 22 of REALHD_PSW8_25MG.bin) and streams it over
 * I2C to the controller on every boot. tools/poe_extract lifts that image out
 * of the stock image and the build links it in as poe_firmware[].
 *
 * I2C access uses the switch's own I2C master at registers 0x0418/0x0424, the
 * block documented at the top of sfp_read_block(). A transfer is one 32-bit
 * descriptor in RTL837X_REG_I2C_CTRL, a register byte in RTL837X_REG_I2C_IN
 * and up to 16 payload bytes in RTL837X_REG_I2C_OUT. Bit 0 of the control
 * register is the busy/direction bit and bit 1 the NAK flag; the read path is
 * the one proven by the SFP EEPROM code.
 *
 * The download itself follows what the vendor loader does: it selects the
 * controller's 0xf4 window and hands the image length and a flash pointer to
 * the PSE serial layer, which streams the image and then reads the 0xec
 * status register. The per-chunk framing of that layer is not decoded, so this
 * driver uses the natural equivalent on the same window: one 16-byte register
 * write per chunk, with the status register read back after each chunk. The
 * window is expected to auto-increment; this is provisional until it has been
 * checked against a PSE on hardware.
 *
 * Selected by MACHINE_POE_CHIP == RTL8238C in the Makefile.
 */
#pragma codeseg BANK3
#pragma constseg BANK3

#include <stdint.h>
#include <stdbool.h>

#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_common.h"
#include "boot.h"
#include "machine.h"
#include "poe.h"


/* The PSE takes a single register byte, so the EEPROM-style wide-address flag
 * stays clear. */
#define POE_ADDR_WIDTH_FLAG	0

static __xdata uint8_t poe_buf[POE_XFER_MAX];
static __xdata uint8_t poe_addr;
static __xdata bool poe_bus_muxed;


/* setup_i2c() only muxes the SFP buses; the PSE bus is a plain GPIO pair until
 * its I2C function is selected. */
static void poe_bus_setup(void)
{
	if (poe_bus_muxed)
		return;

	setup_i2c_bus(poe_cfg.i2c_bus);
	poe_bus_muxed = true;
}


void poe_bringup(void) __banked
{
}

void poe_port_set(uint8_t port_num, uint8_t port_on) __banked
{
}

void poe_global_set(uint8_t on) __banked
{
	(void) on;
}


bool poe_chip_init(void) __banked
{
	poe_bus_setup();
	return false;

}

void poe_init(void) __banked
{
	print_string("\nPoE: loading RTL8238C firmware (");
	print_short(poe_firmware_len);
	print_string(" bytes): ");

	if (poe_chip_init())
		print_string("ok\n");
	else
		print_string("failed\n");
}

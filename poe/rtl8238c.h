#ifndef _RTL8238C_H_
#define _RTL8238C_H_

#include <stdint.h>

/* Registers used by the vendor boot loader: 0xec is the control/status
 * register read after a transfer, 0xf4 the window the firmware image is
 * streamed through. */
#define POE_REG_STATUS		0xec
#define POE_REG_WINDOW		0xf4

/* Bytes per transfer: the I2C controller's data buffer is 16 bytes wide. */
#define POE_XFER_MAX		16

/* Bits of the low byte of RTL837X_REG_I2C_CTRL */
#define POE_CTRL_BUSY		0x1
#define POE_CTRL_NAK		0x2

/* A PSE that stops answering must not hang the boot: give up after this many
 * polls of the control register (sfp_read_block() waits forever). */
#define POE_POLL_LIMIT		0xffff

/* Ports driven by one controller: the 7-bit device address selects the port
 * bank, base address = ports 0-3, base + 1 = ports 4-7. */
#define POE_PORTS_PER_CTRL	4

struct poe_cfg {
	uint8_t i2c_bus;		// I2C bus, I2CBUS() encoded (SCL/SDA pin)
	uint8_t i2c_dev_addr_base;	// 7-bit address of the first controller
};

/* Firmware image generated from the stock firmware by tools/poe_extract and
 * linked into BANK3 together with the driver. */
extern __code const uint8_t poe_firmware[];
extern __code const uint16_t poe_firmware_len;
extern __code const struct poe_cfg poe_cfg;

#endif

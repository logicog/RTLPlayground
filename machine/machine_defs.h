#ifndef _MACHINE_DEFS_H_
#define _MACHINE_DEFS_H_

/*
 * Shared prelude for the per-machine board definitions in this directory.
 *
 * Each .c file in this directory defines exactly one board's
 * `__code const struct machine machine`, and is named after the MACHINE_* symbol
 * it belongs to. The Makefile picks the file matching $(MACHINE), so no #if/#elif
 * selection happens here.
 */

#include "machine.h"
#include "rtl837x_pins.h"
#include "rtl837x_leds.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_common.h"

#define I2CBUS_INVALID (7)
// This is a MACRO to check if I2C_BUS_FROM_SDA_PIN or I2C_BUS_FROM_SCL_PIN don't return I2CBUS_INVALID.
// When I2CBUS_INVALID is returned, the compiler stops with
// "error 176: sizeof applied to an incomplete type" pointing at the offending .i2c line.
#define I2CBUS_VALID(bus) (0 * sizeof(char[(bus) != I2CBUS_INVALID ? 1 : 0]))
#define I2CBUS(sda, scl) \
	((uint8_t)((I2C_BUS_FROM_SCL_PIN(scl) << RTL837X_REG_I2C_SCL_SHIFT) | \
		   (I2C_BUS_FROM_SDA_PIN(sda) << RTL837X_REG_I2C_SDA_SHIFT) | \
		   I2CBUS_VALID(I2C_BUS_FROM_SCL_PIN(scl)) | \
		   I2CBUS_VALID(I2C_BUS_FROM_SDA_PIN(sda))))

#define I2C_BUS_FROM_SDA_PIN(sda_pin) \
	((sda_pin) == GPIO47_I2C_SDA0 ? 0 : \
	 (sda_pin) == GPIO49_I2C_SDA1 ? 1 : \
	 (sda_pin) == GPIO51_I2C_SDA2_UART1_RX ? 2 : \
	 (sda_pin) == GPIO41_I2C_SDA3_MDIO1 ? 3 : \
	 (sda_pin) == GPIO39_I2C_SDA4 ? 4 : I2CBUS_INVALID)

#define I2C_BUS_FROM_SCL_PIN(scl_pin) \
	((scl_pin) == GPIO46_I2C_SCL0 ? 0 : \
	 (scl_pin) == GPIO48_I2C_SCL1 ? 1 : \
	 (scl_pin) == GPIO50_I2C_SCL2_UART1_TX ? 2 : \
	 (scl_pin) == GPIO40_I2C_SCL3_MDC1 ? 3 : I2CBUS_INVALID)

#endif

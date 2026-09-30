#include "rtl837x_pins.h"
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_i2c.h"
#include "machine.h"

extern __code const struct machine machine;

#pragma codeseg BANK2
#pragma constseg BANK2


__xdata uint8_t i2c_buf[16];	/* scratch for one I2C transaction, the controller reads at most 16 bytes */

/*
 * Read up to 16 consecutive registers of the EEPROM via I2C into i2c_buf
 */
bool i2c_read(uint8_t sds, uint8_t dev, uint8_t reg, uint8_t len) __banked __reentrant
{
	uint8_t val;

	len--;
	if (len > 15)
		return false;

	REG_WRITE(RTL837X_REG_I2C_IN, 0, 0, 0, reg);

	REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00,
		  0x1 << (I2C_MEM_ADDR_WIDTH - 16) | len,
		  (dev >> 5) | machine.sds_settings[sds].sds_settings_t.sfp.i2c,
		  ((dev << 3) & 0xff) | FLAG_I2C_TRIGGER);

	do {
		reg_read(RTL837X_REG_I2C_CTRL);
	} while (SFR_DATA_0 & FLAG_I2C_TRIGGER);

	if (SFR_DATA_0 & FLAG_I2C_FAIL)
		return false;

	for (uint8_t i = 0; i <= len; i++) {
		switch (i & 0x3) {
		case 0:
			reg_read(RTL837X_REG_I2C_OUT + i);
			val = SFR_DATA_0;
			break;
		case 1:
			val = SFR_DATA_8;
			break;
		case 2:
			val = SFR_DATA_16;
			break;
		default:
			val = SFR_DATA_24;
			break;
		}
		i2c_buf[i] = val;
	}

	return true;
}

/*
 * Write up to 16 consecutive registers of the EEPROM via I2C into i2c_buf
 */
bool i2c_write(uint8_t sds, uint8_t dev, uint8_t len) __banked __reentrant
{
	uint8_t val;

	len--;
	if (len > 15)
		return false;

	uint8_t i = 0;
	do {
		for (uint8_t cnt = 0; cnt < 4; cnt++) {
			val = i2c_buf[i++];
			switch(cnt)
			{
			case 0:
				SFR_DATA_0 = val;
				break;
			case 1:
				SFR_DATA_8 = val;
				break;
			case 2:
				SFR_DATA_16 = val;
				break;
			default:
				SFR_DATA_24 = val;
				break;
			}
		}
	} while (i <= len);

	REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00,
		  len,
		  (dev >> 5) | machine.sds_settings[sds].sds_settings_t.sfp.i2c,
		  ((dev << 3) & 0xff) | I2C_FLAG_OPPR_WRITE | FLAG_I2C_TRIGGER);

	do {
		reg_read(RTL837X_REG_I2C_CTRL);
	} while (SFR_DATA_0 & FLAG_I2C_TRIGGER);

	if (SFR_DATA_0 & FLAG_I2C_FAIL)
		return false;

	return true;
}
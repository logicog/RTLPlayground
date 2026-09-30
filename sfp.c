#pragma codeseg BANK3
#pragma constseg BANK3

#include <8051.h>
#include <stdint.h>
#include <stdbool.h>

#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_common.h"
#include "rtl837x_pins.h"
#include "rtl837x_phy.h"
#include "rtl837x_port.h"
#include "rtl837x_i2c.h"
#include "machine.h"
#include "phy.h"
#include "boot.h"
#include "sfp.h"

extern __code const struct machine machine;

// SFP1 b0 = 1 => module missing, b1 = 1 => LOS;
// SFP2 b4 = 1 => module missing, b5 = 1 => LOS;
extern volatile __xdata uint32_t ticks;

__xdata uint8_t sfp_pins_last;
__xdata char sfp_module_vendor[2][17];
__xdata char sfp_module_model[2][17];
__xdata char sfp_module_serial[2][17];
__xdata uint8_t sfp_options[2];
__xdata uint8_t sfp_speed[2];
__xdata uint8_t sfp_quirks[2];
__xdata uint8_t sfp_wake_at[2];
__xdata uint8_t sfp_wake_pending[2];
extern __xdata uint8_t sfr_data[4];


__code enum sfp_quirk {
	SFP_QUIRK_DDM = (1 << 0),
};

struct sfp_quirk_entry {
	__code const char *vendor; // Set vendor or model to 0 to act as wildcard
	__code const char *model;
	uint8_t quirks;
};

static __code const struct sfp_quirk_entry sfp_quirk_table[] = {
	{ "QSFPTEK", "QT-SFP+-T", SFP_QUIRK_DDM },
};

enum ephy_mode {
	MDIO_NONE,
	MDIO_DIRECT,
	MDIO_I2C_C22,
	MDIO_I2C_C45,
	MDIO_ROLLBALL,
};

enum sm_sds {
	SM_WAIT,
};

struct sds_rt {
	enum sm_sds status;
	enum ephy_mode phy_mode;
};



static inline uint8_t sfp_rate_to_sds_config(uint8_t rate)
{
	if (rate == 0x1 || rate == 0x2)
		return SDS_100FX;
	if (rate == 0xc || rate == 0xd)
		return SDS_1000BASEX;
	if (rate >= 0x19 && rate <= 0x20)  // Ethernet 2.5 GBit
		return SDS_2500BASEX;
	if (rate >= 0x62 && rate < 0x70)
		return SDS_10GBASER;
	return 0xff;
}


#define I2C_READ_CNT 16
/* Validates the SFP-EEPROM data checksum
 * Input:
 * - sds, sds port which the SFP is connected to.
 * - mem_range:
 *   - false = BASE ID FIELDS: 0..65
 *   - true  = EXTENDED ID FIELDS: 64..95
 */
static int8_t sfp_check(uint8_t sds, __xdata bool mem_range)
{
	uint8_t addr = 0;
	uint8_t len;

	if (mem_range)
		addr = 64;

	uint8_t check = 0;
	do {
		if (!sfp_read_block(sds, addr, I2C_READ_CNT))
			return -1;
		len = I2C_READ_CNT;
		__xdata uint8_t * b = &i2c_buf;
		do {
			check += *b++;
		} while(--len);

		len = 64;
		if (addr >= 64)
			len = 96;
		addr += I2C_READ_CNT;
	} while(addr < len);

	uint8_t crc = i2c_buf[15];
	check -= crc;

	return (check == crc);
}

bool sfp_print_info(uint8_t sfp) __banked
{
	// This loops over the Vendor-name, Vendor OUI, Vendor PN and Vendor rev ASCII fields
	for (uint8_t i = 16; i < 64; i++) {
		if (!(i & 0xf) && !sfp_read_block(sfp, i, 16))
			return false;
		if (i < 20 || i >= 60 || (i >= 36 && i < 40)) // Skip Non-ASCII codes
			continue;
		uint8_t c = i2c_buf[i & 0xf];
		if (c)
			write_char(c);
	}
	print_string("\n");

	return true;
}


// Normalize strings from EEPROM by removing any trailing spaces; this allows simpler comparisons
bool sfp_read_field(__xdata char *dst, uint8_t sfp, uint8_t start, uint8_t length) __banked __reentrant
{
	if (!sfp_read_block(sfp, start, length))
		return false;

	dst[length] = NUL;
	for (uint8_t i = 0; i < length; i++) {
		uint8_t c = i2c_buf[i];
		if (c && (c < 0x20 || c > 0x7e || c == '"' || c == '\\'))
			c = '.';
		dst[i] = c;
	}

	while (length-- > 0 && (dst[length] == ' ' || dst[length] == NUL))
		dst[length] = NUL;

	return true;
}



bool sfp_get_info(uint8_t sfp) __banked
{
	if (!sfp_read_field(sfp_module_vendor[sfp], sfp, 20, 16))
		return false;
	if (!sfp_read_field(sfp_module_model[sfp], sfp, 40, 16))
		return false;

	return sfp_read_field(sfp_module_serial[sfp], sfp, 68, 16);
}



void sfp_apply_quirks(uint8_t sfp) __banked __reentrant
{
	sfp_quirks[sfp] = 0;

	for (uint8_t i = 0; i < sizeof(sfp_quirk_table) / sizeof(*sfp_quirk_table); i++) {
		if (!sfp_quirk_table[i].vendor || !strcmp(sfp_module_vendor[sfp], sfp_quirk_table[i].vendor)) {
			if (!sfp_quirk_table[i].model || !strcmp(sfp_module_model[sfp], sfp_quirk_table[i].model)) {
				sfp_quirks[sfp] |= sfp_quirk_table[i].quirks;
			}
		}
	}

	if (sfp_quirks[sfp] & SFP_QUIRK_DDM) {
		if (!(sfp_options[sfp] & 0x40)) {
			// The module reports that DDM is not implemented, but try a dummy read to confirm
			// 0xff would mean a failed I2C read or an impossible (per spec) voltage greater than 6.5V
			if (sfp_read_block(sfp, 226, 1) && i2c_buf[0] != 0xff) {
				sfp_options[sfp] |= 0x40;
			}
		}
	}
}


/* Inititalize SFP GPIOs */
void setup_sfp_gpio(void) __banked
{
	for (uint8_t sfp = 0; sfp < 2; sfp++) {
		if (machine.sds_settings[sfp].usage != SDS_SFP)
			continue;
		gpio_input_setup(machine.sds_settings[sfp].sds_settings_t.sfp.pin_detect);
		gpio_input_setup(machine.sds_settings[sfp].sds_settings_t.sfp.pin_los);
		gpio_output_setup(machine.sds_settings[sfp].sds_settings_t.sfp.pin_tx_disable, 0);
	}
}

/* Read SFP-PHY with C45
 * arguments:
 *   - sds: SDS port
 *   - dev: I2C-address
 *   - devad: set -1 for C22 read.
 *   - reg: 16-bit for C45, lsb for C22
*/
// https://elixir.bootlin.com/linux/v7.2.8/source/drivers/net/phy/phy_device.c#L991
bool i2c_mdio_phy_read_c45(uint8_t sds, uint8_t phy_id, int8_t devad, uint16_t reg) __reentrant __banked
{
	print_string("i2c: sds: "); print_byte(sds);
	print_string(" phy_id: "); print_byte(phy_id);
	print_string(" devad: "); print_byte(devad);
	print_string(" reg: "); print_short(reg);

	uint8_t dev = phy_id + 0x40;
	uint8_t len = 0x02;
	if (devad >= 0) {
		sfr_data[3] = devad | 0x20;
		sfr_data[2] = reg >> 8;
		sfr_data[1] = reg;
		len |= 0x3 << 4;
	} else {
		sfr_data[3] = reg;
		sfr_data[2] = 0;
		sfr_data[1] = 0;
		len |= 0x1 << 4;
	}
	sfr_data[0] = 0;

	write_char(' ');
	print_byte(len);
	write_char(' ');
	print_byte(sfr_data[0]);
	print_byte(sfr_data[1]);
	print_byte(sfr_data[2]);
	print_byte(sfr_data[3]);
	write_char('\n');

	reg_write_m(RTL837X_REG_I2C_IN);

	REG_WRITE(RTL837X_REG_I2C_CTRL,
			  0x00,
			  len,
			  (dev >> 5) | machine.sds_settings[sds].sds_settings_t.sfp.i2c,
			  ((dev << 3) & 0xff) | FLAG_I2C_TRIGGER);

	do {
		reg_read(RTL837X_REG_I2C_CTRL);
	} while (SFR_DATA_0 & FLAG_I2C_TRIGGER);

	if (SFR_DATA_0 & FLAG_I2C_FAIL)
		return false;

	reg_read(RTL837X_REG_I2C_OUT);
	i2c_buf[0] = SFR_DATA_0;
	i2c_buf[1] = SFR_DATA_8;

	return true;
}


static bool sfp_module_read(uint8_t sfp)
{
	uint8_t rate;
	int8_t sfp_status;
	bool may_has_phy = false;

	// Validate EEPROM MEMORY: BASIC ID
	sfp_status = sfp_check(sfp, false);
	if (sfp_status < 0)
		return false;
	if (!sfp_status)
		print_string("\nERROR: SFP base checksum failed!\n");

	// Validate EEPROM MEMORY: EXTENDED ID
	sfp_status = sfp_check(sfp, true);
	if (sfp_status < 0)
		return false;
	if (!sfp_status)
		print_string("\nERROR: SFP extended checksum failed!\n");

	// Read Reg 11: Encoding, see SFF-8472 and SFF-8024
	// Read Reg 12: Signalling rate (including overhead) in 100Mbit: 0xd: 1Gbit, 0x67:10Gbit
	if (!sfp_read_block(sfp, 0, 16))
		return false;

	rate = i2c_buf[12];
	if (sfp_speed[sfp] == SFP_SPEED_100M)
		rate = 0x1;
	else if (sfp_speed[sfp] == SFP_SPEED_1G)
		rate = 0xc;
	else if (sfp_speed[sfp] == SFP_SPEED_2G5)
		rate = 0x19;
	else if (sfp_speed[sfp] == SFP_SPEED_10G)
		rate = 0x69;
	print_string("  Rate: "); print_byte(rate);  // Normally 1, but 0 for DAC, can be ignored?
	print_string("  Encoding: "); print_byte(i2c_buf[11]);
	print_string("  Connector: "); print_byte(i2c_buf[2]);
	may_has_phy = i2c_buf[2] == SFF_CONN_REF_RJ45;
	print_string("  Module: ");
	if (!sfp_print_info(sfp))
		return false;
	if (!sfp_read_block(sfp, 64, I2C_READ_CNT))
		return false;

	print_string("  Option: ");
	print_byte(i2c_buf[0]);
	print_byte(i2c_buf[1]);
	
	if (!sfp_read_block(sfp, 80, I2C_READ_CNT))
		return false;
	sfp_options[sfp] = i2c_buf[12];

	print_byte(i2c_buf[12]);
	print_byte(i2c_buf[13]);
	print_string("\n");

	if (!sfp_get_info(sfp))
		return false;

	sfp_apply_quirks(sfp);

	uint8_t sfp_rate = sfp_rate_to_sds_config(rate);

	// Detect SFP PHY
	if (may_has_phy) {
		if (i2c_mdio_phy_read_c45(sfp, SFP_PHY_ADDR, -1, 0x02)) {
			print_string("EPHY FOUND: ID: ");
			print_byte(i2c_buf[0]);
			print_byte(i2c_buf[1]);
			if (i2c_mdio_phy_read_c45(sfp, SFP_PHY_ADDR, -1, 0x03)) {
				print_byte(i2c_buf[0]);
				print_byte(i2c_buf[1]);
			}
			write_char('\n');
			// translate fiber SDS settings to xSGMII variant
			// So we have in-band handling with the phy.
			switch(sfp_rate) {
				case SDS_1000BASEX:
					sfp_rate = SDS_SGMII;
					break;
				case SDS_2500BASEX:
					sfp_rate = SDS_2G5_SGMII;
					break;
				case SDS_10GBASER:
					sfp_rate = SDS_10G_QXGMII;
					break;
				default:
					break;
			}
		}
	}

	sds_config(sfp, sfp_rate);

	return true;
}


void handle_sfp(void) __banked
{
	for (uint8_t sds = 0; sds < 2; sds++) {
		if (machine.sds_settings[sds].usage != SDS_SFP)
			continue;

		__xdata uint8_t port = sds == 1 ? MAC_SDS1 : MAC_SDS0;

		// pin_detect is active_low
		bool sfp_cage_is_empty = gpio_pin_test(machine.sds_settings[sds].sds_settings_t.sfp.pin_detect);

		if (sfp_cage_is_empty) {
			if (!(sfp_pins_last & (0x1 << (sds << 2)))) {
				sfp_pins_last |= 0x01 << (sds << 2);
				sfp_wake_pending[sds] = 0;
				print_string("\n<MODULE REMOVED>  Port: "); print_phys_port(port); write_char('\n');
				sds_config(sds, SDS_OFF);
			}
			continue;
		}

		if (sfp_pins_last & (0x1 << (sds << 2))) {
			sfp_pins_last &= ~(0x01 << (sds << 2));
			print_string("\n<MODULE INSERTED>  Port: "); print_phys_port(port);
			sfp_wake_at[sds] = ticks;
			sfp_wake_pending[sds] = 1;
			continue;
		}

		if (sfp_wake_pending[sds]
				&& (uint8_t)((uint8_t)ticks - sfp_wake_at[sds]) >= SFP_WAKE_TICKS) {
			sfp_wake_pending[sds] = 0;
			if (!sfp_module_read(sds)) {
				print_string("SFP: an I2C read failed, retrying on the next poll\n");
				sfp_pins_last |= 0x01 << (sds << 2);
			}
		} else {
			continue;
		}

		if (!gpio_pin_test(machine.sds_settings[sds].sds_settings_t.sfp.pin_los)) {
			if (sfp_pins_last & (0x2 << (sds << 2))) { // 0x2 0x08
				sfp_pins_last &= ~(0x02 << (sds << 2));
				print_string("\n<SFP-RX OK>  Port: "); print_phys_port(port); write_char('\n');
			}
		} else {
			if (!(sfp_pins_last & 0x2 << (sds << 2))) {
				sfp_pins_last |= 0x02 << (sds << 2);
				print_string("\n<SFP-RX LOS>  Port: "); print_phys_port(port); write_char('\n');
			}
		}
	}
}

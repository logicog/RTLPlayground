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
#include "machine.h"
#include "phy.h"
#include "boot.h"
#include "sfp.h"
#include "timer.h"

extern __code const struct machine machine;

// SFP1 b0 = 1 => module missing, b1 = 1 => LOS;
// SFP2 b4 = 1 => module missing, b5 = 1 => LOS;

__xdata uint8_t sfp_pins_last;
__xdata char sfp_module_vendor[2][17];
__xdata char sfp_module_model[2][17];
__xdata char sfp_module_serial[2][17];
__xdata uint8_t sfp_options[2];
__xdata uint8_t sfp_buf[16];	/* scratch for one I2C transaction, the controller reads at most 16 bytes */
__xdata uint8_t sfp_speed[2];
__xdata uint8_t sfp_quirks[2];
__xdata uint8_t sfp_id[2][2];
__xdata uint8_t sfp_cal[2][36];
__xdata uint8_t sfp_dom[2][16];
__xdata uint8_t sfp_ready;
__xdata struct timer sfp_wake[2];
__xdata uint8_t sfp_wake_pending[2];
static __xdata struct timer sfp_dom_poll;
static __xdata struct timer sfp_io;
static __xdata uint8_t sfp_dom_pending;
static __xdata uint8_t sfp_busy;
static __xdata uint8_t sfp_cur;
static __xdata uint8_t sfp_first;
static __xdata uint8_t sfp_i;
static __xdata uint8_t sfp_rate;


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




static inline uint8_t sfp_rate_to_sds_config(uint8_t rate)
{
	if (rate == 0x1 || rate == 0x2)
		return SDS_100FX;
	if (rate == 0xc || rate == 0xd)
		return SDS_1000BX_FIBER;
	if (rate >= 0x19 && rate <= 0x20)  // Ethernet 2.5 GBit
		return SDS_HSG;
	if (rate >= 0x62 /* && rate < 0x70 */) // Force higher rates to 10G (e.g., 25G, 50G)
		return SDS_10GR;

	return 0xff;
}




// Normalize strings from EEPROM by removing any trailing spaces; this allows simpler comparisons
static void sfp_store_field(__xdata char *dst, uint8_t length) __reentrant
{
	dst[length] = NUL;
	for (uint8_t i = 0; i < length; i++) {
		uint8_t c = sfp_buf[i];
		if (c && (c < 0x20 || c > 0x7e || c == '"' || c == '\\'))
			c = '.';
		dst[i] = c;
	}

	while (length-- > 0 && (dst[length] == ' ' || dst[length] == NUL))
		dst[length] = NUL;
}


static void sfp_lookup_quirks(void)
{
	sfp_quirks[sfp_cur] = 0;

	for (uint8_t i = 0; i < sizeof(sfp_quirk_table) / sizeof(*sfp_quirk_table); i++) {
		if (!sfp_quirk_table[i].vendor || !strcmp(sfp_module_vendor[sfp_cur], sfp_quirk_table[i].vendor)) {
			if (!sfp_quirk_table[i].model || !strcmp(sfp_module_model[sfp_cur], sfp_quirk_table[i].model)) {
				sfp_quirks[sfp_cur] |= sfp_quirk_table[i].quirks;
			}
		}
	}
}


/* Inititalize SFP GPIOs */
void setup_sfp_gpio(void) __banked
{
	for (uint8_t sfp = 0; sfp < machine.n_sfp; sfp++) {
		gpio_input_setup(machine.sfp_port[sfp].pin_detect);
		gpio_input_setup(machine.sfp_port[sfp].pin_los);
		gpio_output_setup(machine.sfp_port[sfp].pin_tx_disable, 0);
	}
	timer_set(&sfp_dom_poll, SFP_DOM_TICKS);
}



#define SFP_STEP_DDM 5
#define SFP_STEP_CAL 6
#define SFP_STEP_DOM 9


static bool sfp_due(void)
{
	for (uint8_t sfp = 0; sfp < machine.n_sfp; sfp++) {
		if (sfp_wake_pending[sfp] && timer_expired(&sfp_wake[sfp])) {
			sfp_wake_pending[sfp] = 0;
			sfp_cur = sfp;
			sfp_first = 0;
			return true;
		}
	}

	if (timer_expired(&sfp_dom_poll)) {
		timer_reset(&sfp_dom_poll);
		sfp_dom_pending = sfp_ready;
	}
	for (uint8_t sfp = 0; sfp < machine.n_sfp; sfp++) {
		if (sfp_dom_pending & sfp_ready & (1 << sfp)) {
			sfp_dom_pending &= ~(1 << sfp);
			if (sfp_options[sfp] & 0x40) {
				sfp_cur = sfp;
				sfp_first = SFP_STEP_DOM;
				return true;
			}
		}
	}

	return false;
}


static void sfp_retry(void)
{
	print_string("SFP: an I2C read failed, retrying on the next poll\n");
	sfp_pins_last |= 0x01 << (sfp_cur << 2);
}


static __code const uint8_t sfp_steps[][2] = {
	{ 11, 2 }, { 92, 1 }, { 20, 16 }, { 40, 16 }, { 68, 16 }, { 226, 1 },
	{ 184, 16 }, { 200, 16 }, { 216, 4 }, { 224, 16 }
};

#define SFP_STEPS (sizeof(sfp_steps) / sizeof(*sfp_steps))


static bool sfp_skip(void)
{
	uint8_t options = sfp_options[sfp_cur];

	// The module reports that DDM is not implemented, but try a dummy read to confirm
	if (sfp_i == SFP_STEP_DDM)
		return !(sfp_quirks[sfp_cur] & SFP_QUIRK_DDM) || (options & 0x40);
	if (sfp_i == SFP_STEP_DOM)
		return !(options & 0x40);
	if (sfp_i >= SFP_STEP_CAL)
		return (options & 0x50) != 0x50;
	return false;
}


static void sfp_step(void)
{
	switch (sfp_i) {
	case 0:
		// Read Reg 11: Encoding, see SFF-8472 and SFF-8024
		// Read Reg 12: Signalling rate (including overhead) in 100Mbit: 0xd: 1Gbit, 0x67:10Gbit
		sfp_id[sfp_cur][0] = sfp_buf[0];
		sfp_id[sfp_cur][1] = sfp_buf[1];
		sfp_rate = sfp_buf[1];
		if (sfp_speed[sfp_cur] == SFP_SPEED_100M)
			sfp_rate = 0x1;
		else if (sfp_speed[sfp_cur] == SFP_SPEED_1G)
			sfp_rate = 0xc;
		else if (sfp_speed[sfp_cur] == SFP_SPEED_2G5)
			sfp_rate = 0x19;
		else if (sfp_speed[sfp_cur] == SFP_SPEED_10G)
			sfp_rate = 0x69;
		break;
	case 1:
		sfp_options[sfp_cur] = sfp_buf[0];
		break;
	case 2:
		sfp_store_field(sfp_module_vendor[sfp_cur], 16);
		break;
	case 3:
		sfp_store_field(sfp_module_model[sfp_cur], 16);
		break;
	case 4:
		sfp_store_field(sfp_module_serial[sfp_cur], 16);
		sfp_lookup_quirks();
		break;
	case SFP_STEP_DDM:
		// 0xff would mean a failed I2C read or an impossible (per spec) voltage greater than 6.5V
		if (sfp_buf[0] != 0xff)
			sfp_options[sfp_cur] |= 0x40;
		break;
	case SFP_STEP_DOM:
		memcpy(sfp_dom[sfp_cur], sfp_buf, sfp_steps[sfp_i][1]);
		break;
	default:
		memcpy(sfp_cal[sfp_cur] + ((sfp_i - SFP_STEP_CAL) << 4), sfp_buf, sfp_steps[sfp_i][1]);
	}
}


void sfp_task(void) __banked
{
	if (!sfp_busy) {
		if (!sfp_due())
			return;
		sfp_i = sfp_first;
	}

	for (;;) {
		if (sfp_busy) {
			if (sfp_read_busy() && !timer_expired(&sfp_io))
				return;
			sfp_busy = 0;
			if (sfp_read_end(sfp_steps[sfp_i][1])) {
				sfp_step();
			} else if (sfp_i != SFP_STEP_DDM) {
				if (!sfp_first)
					sfp_retry();
				return;
			}
			sfp_i++;
		}
		while (sfp_i < SFP_STEPS && sfp_skip())
			sfp_i++;
		if (sfp_i >= SFP_STEPS)
			break;
		sfp_read_start(sfp_cur, sfp_steps[sfp_i][0], sfp_steps[sfp_i][1]);
		timer_set(&sfp_io, SFP_IO_TICKS);
		sfp_busy = 1;
	}

	if (sfp_first || (sfp_pins_last & (0x1 << (sfp_cur << 2))))
		return;
	print_string("  Rate: "); print_byte(sfp_rate);  // Normally 1, but 0 for DAC, can be ignored?
	print_string("  Encoding: "); print_byte(sfp_id[sfp_cur][0]);
	print_string("  Module: "); print_string_x(sfp_module_vendor[sfp_cur]); write_char(' ');
	print_string_x(sfp_module_model[sfp_cur]); write_char(' ');
	print_string_x(sfp_module_serial[sfp_cur]); write_char('\n');
	sfp_ready |= 1 << sfp_cur;
	sds_config(machine.sfp_port[sfp_cur].sds, sfp_rate_to_sds_config(sfp_rate));
}


void handle_sfp(void) __banked
{
	for (uint8_t sfp = 0; sfp < machine.n_sfp; sfp++) {
		if (!gpio_pin_test(machine.sfp_port[sfp].pin_detect)) {
			if (sfp_pins_last & (0x1 << (sfp << 2))) {
				sfp_pins_last &= ~(0x01 << (sfp << 2));
				sfp_ready &= ~(1 << sfp);
				print_string("\n<MODULE INSERTED>  Slot: "); write_char('1' + sfp);
				timer_set(&sfp_wake[sfp], SFP_WAKE_TICKS);
				sfp_wake_pending[sfp] = 1;
			}
		} else {
			if (!(sfp_pins_last & (0x1 << (sfp << 2)))) {
				sfp_pins_last |= 0x01 << (sfp << 2);
				sfp_wake_pending[sfp] = 0;
				sfp_ready &= ~(1 << sfp);
				print_string("\n<MODULE REMOVED>  Slot: "); write_char('1' + sfp); write_char('\n');
			}
		}

		if (!gpio_pin_test(machine.sfp_port[sfp].pin_los)) {
			if (sfp_pins_last & (0x2 << (sfp << 2))) { // 0x2 0x08
				sfp_pins_last &= ~(0x02 << (sfp << 2));
				print_string("\n<SFP-RX OK>  Slot: "); write_char('1' + sfp); write_char('\n');
			}
		} else {
			if (!(sfp_pins_last & 0x2 << (sfp << 2))) {
				sfp_pins_last |= 0x02 << (sfp << 2);
				print_string("\n<SFP-RX LOS>  Slot: "); write_char('1' + sfp); write_char('\n');
			}
		}
	}
}

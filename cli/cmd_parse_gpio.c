/*
 * "gpio" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_gpio.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"

#pragma codeseg BANK2
#pragma constseg BANK2

__xdata uint8_t gpio_last_value[8] = { 0 };

// Print GPIO status
void cmd_parse_gpio(void) {
	for (uint8_t idx = 0; idx < 2; idx++) {
		reg_read(RTL837X_REG_GPIO_00_31_INPUT + (idx * 4));
		print_string("GPIO ");
		write_char(idx + '0');
		write_char(':');
		write_char(' ');

		print_byte(SFR_DATA_24);
		print_byte(SFR_DATA_16);
		print_byte(SFR_DATA_8);
		print_byte(SFR_DATA_0);

		write_char(' ');
		print_byte( gpio_last_value[(idx *4)] ^ SFR_DATA_24);
		gpio_last_value[(idx *4)] = SFR_DATA_24;
		print_byte( gpio_last_value[(idx *4) + 1] ^ SFR_DATA_16);
		gpio_last_value[(idx *4) + 1] = SFR_DATA_16;
		print_byte( gpio_last_value[(idx *4) + 2] ^ SFR_DATA_8);
		gpio_last_value[(idx *4) + 2] = SFR_DATA_8;
		print_byte( gpio_last_value[(idx *4) + 3] ^ SFR_DATA_0);
		gpio_last_value[(idx *4) + 3] = SFR_DATA_0;
		write_char('\n');
	}
}

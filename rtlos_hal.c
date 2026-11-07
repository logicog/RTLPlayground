#include "rtlos_hal.h"
#include "rtl837x_common.h"
#include "rtl837x_regs.h"

/* The GPIO registers come in a block for pins 0-31 and one for pins 32-63 */
#define gpio_pull_reg(reg_00_31, reg_32_63, pin) (pin < 32 ? reg_00_31 : reg_32_63)

#pragma codeseg BANK2
#pragma constseg BANK2

/*
 * Configure the internal pull resistor of a GPIO pin
 * pin: GPIO pin number 0-63
 * mode: GPIO_PULL_NONE, GPIO_PULL_UP or GPIO_PULL_DOWN
 */
void set_gpio_pull_mode(uint8_t pin, uint8_t mode) __banked
{
	uint8_t bit;
	uint16_t pull_en;
	uint16_t pull_up;
	uint16_t pull_down;

	if (pin > 63) {
		return;
	}

	bit = pin % 32;
	pull_en = gpio_pull_reg(RTL837X_REG_GPIO_00_31_PULL_EN,
				RTL837X_REG_GPIO_32_63_PULL_EN, pin);
	pull_up = gpio_pull_reg(RTL837X_REG_GPIO_00_31_PULL_UP,
				RTL837X_REG_GPIO_32_63_PULL_UP, pin);
	pull_down = gpio_pull_reg(RTL837X_REG_GPIO_00_31_PULL_DOWN,
				  RTL837X_REG_GPIO_32_63_PULL_DOWN, pin);

	switch (mode) {
	case GPIO_PULL_NONE:
		reg_bit_clear(pull_up, bit);
		reg_bit_clear(pull_down, bit);
		reg_bit_clear(pull_en, bit);
		break;
	case GPIO_PULL_UP:
		reg_bit_set(pull_up, bit);
		reg_bit_clear(pull_down, bit);
		reg_bit_set(pull_en, bit);
		break;
	case GPIO_PULL_DOWN:
		reg_bit_clear(pull_up, bit);
		reg_bit_set(pull_down, bit);
		reg_bit_set(pull_en, bit);
		break;
	default:
		break;
	}
}

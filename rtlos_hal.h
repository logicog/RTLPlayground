#ifndef _RTLOS_HAL_H_
#define _RTLOS_HAL_H_

#include <stdint.h>

/* Values for the mode argument of set_gpio_pull_updown() */
#define GPIO_PULL_NONE	0
#define GPIO_PULL_UP	1
#define GPIO_PULL_DOWN	2

/*
 * Configure the internal pull resistor of a GPIO pin
 * pin: GPIO pin number 0-63
 * mode: GPIO_PULL_NONE to disable pull up and pull down,
 *       GPIO_PULL_UP to enable the pull up,
 *       GPIO_PULL_DOWN to enable the pull down
 */
void set_gpio_pull_mode(uint8_t pin, uint8_t mode) __banked;

#endif

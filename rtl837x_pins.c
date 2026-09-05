#include "rtl837x_pins.h"
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "machine.h"

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];

#pragma codeseg BANK2
#pragma constseg BANK2

uint8_t i2c_bus_from_sda_pin(uint8_t sda_pin) __banked {
	switch (sda_pin) {
		case GPIO47_I2C_SDA0:
			return 0;
		case GPIO49_I2C_SDA1:
			return 1;
		case GPIO51_I2C_SDA2_UART1_RX:
			return 2;
		case GPIO41_I2C_SDA3_MDIO1:
			return 3;
		case GPIO39_I2C_SDA4:
			return 4;
		default:
			return 0xFF;
	}
}

uint8_t i2c_bus_from_scl_pin(uint8_t scl_pin) __banked{
	switch (scl_pin) {
		case GPIO46_I2C_SCL0:
			return 0;
		case GPIO48_I2C_SCL1:
			return 1;
		case GPIO50_I2C_SCL2_UART1_TX:
			return 2;
		case GPIO40_I2C_SCL3_MDC1:
			return 3;
		default:
			return 0xFF;
	}
}

/* Returns RTL837X_REG_GPIO_XX_OUTPUT register address */
static uint16_t gpio_output_reg(uint8_t pin) __banked{
	return pin < 32 ? RTL837X_REG_GPIO_00_31_OUTPUT : RTL837X_REG_GPIO_32_63_OUTPUT;
} 

/* Returns RTL837X_REG_GPIO_XX_DIRECTION register address */
static uint16_t gpio_direction_reg(uint8_t pin) __banked {
	return pin < 32 ? RTL837X_REG_GPIO_00_31_DIRECTION : RTL837X_REG_GPIO_32_63_DIRECTION;
} 


/* Enable GPIO functions for pin */
static void gpio_mux_setup(uint8_t pin) __banked
{
	// Some GPIOs require setting MUX registers to enable GPIO
	switch (pin) {
		case GPIO10_LED10:
			reg_bit_clear(RTL837X_PIN_MUX_0, 10);
			break;
		case GPIO30_ACL_BIT3_EN:
			reg_bit_clear(RTL837X_PIN_MUX_2, 3);
			break;
		case GPIO36_PWM_OUT:
			reg_bit_set(RTL837X_PIN_MUX_1, 30);
			break;
		case GPIO37:
		case GPIO38:
			// Intentionally empty, always GPIO
			break;
		case GPIO46_I2C_SCL0:
			// Bit 7-8 0b00 -> GPIO
			reg_read_m(RTL837X_PIN_MUX_1);
			sfr_mask_data(0, 0x80, 0x00);
			sfr_mask_data(1, 0x01, 0x00);
			reg_write_m(RTL837X_PIN_MUX_1);	
			break;
		case GPIO50_I2C_SCL2_UART1_TX:
			// Bit 15-16 0b00 -> GPIO
			reg_read_m(RTL837X_PIN_MUX_1);
			sfr_mask_data(1, 0x80, 0x00);
			sfr_mask_data(2, 0x01, 0x00);
			reg_write_m(RTL837X_PIN_MUX_1);	
			break;
		case GPIO51_I2C_SDA2_UART1_RX:
			// Bit 17-18 0b00 -> GPIO
			reg_read_m(RTL837X_PIN_MUX_1);
			sfr_mask_data(2, 0x06, 0x00);
			reg_write_m(RTL837X_PIN_MUX_1);
			break;
		case GPIO54_ACL_BIT2_EN:
			reg_bit_clear(RTL837X_PIN_MUX_2, 2);
			break;
		case GPIO_NA:
			print_string("Attemped to assign GPIO function to N/A pin!");
			break;
		default:
			print_string("GPIO MUX setup not implemented for pin="); print_byte(pin); print_string("\n");
	}
}

void gpio_input_setup(uint8_t pin) __banked {
	if (pin == GPIO_NA) { 
		return;
	}

	gpio_mux_setup(pin);
	reg_bit_clear(gpio_direction_reg(pin), (pin % 32));
}

void gpio_output_setup(uint8_t pin, __xdata uint8_t initial_val) __banked{
	if (pin == GPIO_NA) { 
		return;
	}
	gpio_mux_setup(pin);

	// We need to setup value before enabling output on PIN
	if (initial_val) {
		reg_bit_set(gpio_output_reg(pin), (pin % 32));
	} else {
		reg_bit_clear(gpio_output_reg(pin), (pin % 32));
	}

	reg_bit_set(gpio_direction_reg(pin), (pin % 32));
}


/*
 * Read up to 16 consecutive registers of the EEPROM via I2C into sfp_buf
 */
bool sfp_read_block(uint8_t slot, uint8_t reg, uint8_t len) __banked __reentrant
{
	uint8_t dev;
	uint8_t val;

	len--;
	if (len > 15)
		return false;

	dev = (reg & 0x80) ? 0x51 : 0x50;	// 0x51 holds the diagnostics, 0x50 the module data
	reg &= 0x7f;

	REG_WRITE(RTL837X_REG_I2C_IN, 0, 0, 0, reg);

	REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00,
		  0x1 << (I2C_MEM_ADDR_WIDTH - 16) | len,
		  (dev >> 5) | i2c_bus_from_scl_pin(machine.sfp_port[slot].i2c.scl) << 5
		  | i2c_bus_from_sda_pin(machine.sfp_port[slot].i2c.sda) << 2,
		  ((dev << 3) & 0xff) | 0x1);

	do {
		reg_read(RTL837X_REG_I2C_CTRL);
	} while (SFR_DATA_0 & 0x1);

	if (SFR_DATA_0 & 0x2)
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
		sfp_buf[i] = val;
	}

	return true;
}

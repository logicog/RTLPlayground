#ifndef _RTL837X_PINS_H_
#define _RTL837X_PINS_H_

#include <stdint.h>

#define GPIO0_LED0                0
#define GPIO1_LED1                1
#define GPIO2_LED2                2
#define GPIO3_LED3                3
#define GPIO4_LED4                4
#define GPIO5_LED5                5
#define GPIO6_LED6                6
#define GPIO7_LED7                7
#define GPIO8_LED8                8
#define GPIO9_LED9                9
#define GPIO10_LED10              10
#define GPIO11_LED11              11
#define GPIO12_LED12              12
#define GPIO13_LED13              13
#define GPIO14_LED14              14
#define GPIO15_LED15              15
#define GPIO16_LED16              16
#define GPIO17_LED17              17
#define GPIO18_LED18              18
#define GPIO19_LED19              19
#define GPIO20_LED20              20
#define GPIO21_LED21              21
#define GPIO22_LED22              22
#define GPIO23_LED23              23
#define GPIO24_LED24              24
#define GPIO25_LED25              25
#define GPIO26_LED26              26
#define GPIO27_LED27              27
#define GPIO28_SYS_LED            28
#define GPIO29_GLB_RLDP_LED_EN    29
#define GPIO30_ACL_BIT3_EN        30
#define GPIO31_UART0_TX           31
#define GPIO32_UART0_RX           32
#define GPIO33_INT                33
#define GPIO34_MDC0               34
#define GPIO35_MDIO0              35
#define GPIO36_PWM_OUT            36
#define GPIO37                    37
#define GPIO38                    38
#define GPIO39_I2C_SDA4           39
#define GPIO40_I2C_SCL3_MDC1      40
#define GPIO41_I2C_SDA3_MDIO1     41
#define GPIO42_SPI                42
#define GPIO43_SPI                43
#define GPIO44_SPI                44
#define GPIO45_SPI                45
#define GPIO46_I2C_SCL0           46
#define GPIO47_I2C_SDA0           47
#define GPIO48_I2C_SCL1           48
#define GPIO49_I2C_SDA1           49
#define GPIO50_I2C_SCL2_UART1_TX  50
#define GPIO51_I2C_SDA2_UART1_RX  51
#define GPIO52_ACL_BIT0_EN        52
#define GPIO53_ACL_BIT1_EN        53
#define GPIO54_ACL_BIT2_EN        54
#define GPIO55_PTP_CLK_IN         55
#define GPIO56_PTP_CLK_OUT        56
#define GPIO57_PTP_TOD_OUT        57
#define GPIO58_PTP_PPS_OUT        58
#define GPIO59_PTP_TOD_IN         59
#define GPIO60_PTP_PPS_IN         60
#define GPIO61_SYNCELOCK0         61
#define GPIO62_SYNCELOCK1         62
#define GPIO63_MDIO               63

/* Not available GPIO */
#define GPIO_NA                   0xFF

/* Convert SDA PIN GPIO to I2C bus number */
uint8_t i2c_bus_from_sda_pin(uint8_t sda_pin);

/* Convert SCL PIN GPIO to I2C bus number */
uint8_t i2c_bus_from_scl_pin(uint8_t scl_pin);

/*
 * Setup a GPIO pin as input
 * pin: GPIO pin number 0-63
 */
void gpio_input_setup(uint8_t pin);

/*
 * Setup a GPIO pin as output
 * pin: GPIO pin number 0-63
 * initial_val: 1 for bit set in RTL837X_REG_GPIO_xx_OUTPUT, 0 for bit not set
 */
void gpio_output_setup(uint8_t pin, __xdata uint8_t initial_val);

#endif

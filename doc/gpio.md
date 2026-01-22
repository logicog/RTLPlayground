
# GPIO Pin, Function and MUX registers.

These functions should bevalid for `RTL8372`, `RTL8372N`, `RTL8373`, and `RTL8373N`.

`N`-version doesn't seems to have all the GPIO pins available on the outside of the package.

| GPIO | Function | TYPE | MUX REG, BIT | (RTL8372) PIN# | (RTL8372N) PIN# |
| ----- | ---- | ---- |  ---- | ---- | ---- |
| GPIO0 | LED0 | I/OPU | IO_MUX_SEL_0, BIT 0 | G1 | 12 |
| GPIO1 | LED1 | I/OPU | IO_MUX_SEL_0, BIT 1 | G2 | 15 |
| GPIO2 | LED2 | I/OPU | IO_MUX_SEL_0, BIT 2 | G3 | 14 |
| GPIO3 | LED3 | I/OPU | IO_MUX_SEL_0, BIT 3 | H1 | 16 |
| GPIO4 | LED4 | I/OPU | IO_MUX_SEL_0, BIT 4 | H2 | 18 |
| GPIO5 | LED5 | I/OPU | IO_MUX_SEL_0, BIT 5 | H3 | 20 |
| GPIO6 | LED6 | I/OPU | IO_MUX_SEL_0, BIT 6 | J1 | 22 |
| GPIO7 | LED7 | I/OPU | IO_MUX_SEL_0, BIT 7 | J2 | NoPin? |
| GPIO8 | LED8 | I/OPU | IO_MUX_SEL_0, BIT 8 | J3 | 24 |
| GPIO9 | LED9 | I/OPD | IO_MUX_SEL_0, BIT 9 | L1 | 23 |
| GPIO10 | LED10 | I/OPU | IO_MUX_SEL_0, BIT 10 | L2 | 26 |
| GPIO11 | LED11 | I/OPU | IO_MUX_SEL_0, BIT 11 | L3 | NoPin? |
| GPIO12 | LED12 | I/OPD | IO_MUX_SEL_0, BIT 12 | M1 | 28 |
| GPIO13 | LED13 | I/OPU | IO_MUX_SEL_0, BIT 13 | M2 | NoPin? |
| GPIO14 | LED14 | I/OPU | IO_MUX_SEL_0, BIT 14 | M3 | NoPin? |
| GPIO15 | LED15 | I/OPU | IO_MUX_SEL_0, BIT 15 | N1 | 25 |
| GPIO16 | LED16 | I/OPU | IO_MUX_SEL_0, BIT 16 | N2 | NoPin? |
| GPIO17 | LED17 | I/OPU | IO_MUX_SEL_0, BIT 17 | N3 | NoPin? |
| GPIO18 | LED18 | I/OPD | IO_MUX_SEL_0, BIT 18 | P1 | 30 |
| GPIO19 | LED19 | I/OPU | IO_MUX_SEL_0, BIT 19 | P2 | NoPin? |
| GPIO20 | LED20 | I/OPU | IO_MUX_SEL_0, BIT 20 | P3 | NoPin? |
| GPIO21 | LED21 | I/OPU | IO_MUX_SEL_0, BIT 21 | R1 | 27 |
| GPIO22 | LED22 | I/OPU | IO_MUX_SEL_0, BIT 22 | R2 | NoPin? |
| GPIO23 | LED23 | I/OPU | IO_MUX_SEL_0, BIT 23 | R3 | NoPin? |
| GPIO24 | LED24 | I/OPU | IO_MUX_SEL_0, BIT 24 | N19 | 88 |
| GPIO25 | LED25 | I/OPU | IO_MUX_SEL_0, BIT 25 | P19 | 86 |
| GPIO26 | LED26 | I/OPU | IO_MUX_SEL_0, BIT 26 | P18 | 84 |
| GPIO27 | LED27 | I/OPU | IO_MUX_SEL_0, BIT 27 | R19 | 82 |
| GPIO28 | SYS_LED | I/OPU | IO_MUX_SEL_0, BIT 28 | F1 | 13 |
| GPIO29 | GLB_RLDP_LED_EN |  | IO_MUX_SEL_0, BIT 29 |  | NoPin? |
| GPIO30 | ACL_BIT3_EN |  | IO_MUX_SEL_2, BIT 3 | F3 | 11 |
| GPIO31 | UART TX (OUTPUT) |  | IO_MUX_SEL_1, BIT 0 | L20 | 90 |
| GPIO32 | UART TX (INPUT) |  | IO_MUX_SEL_1, BIT 1 | L21 |  |
| GPIO33 | GPIO_INT |  | IO_MUX_SEL_1, BIT 2 |  |  |
| GPIO34 | MDC0 |  | IO_MUX_SEL_1, BIT 3 |  |  |
| GPIO35 | MDIO0 |  | IO_MUX_SEL_1, BIT 4 |  |  |
| GPIO36 | PWM_OUT |  | IO_MUX_SEL_1, BIT 30 | B13 |  |
| GPIO37 | --- |  |  | L18 |  |
| GPIO38 | --- |  |  | K19 |  |
| GPIO39 | MSDA4 |  | IO_MUX_SEL_1, BIT 29 | K20 | 95 |
| GPIO40 | MDC1/SCL3 |  | IO_MUX_SEL_1, BIT 5 & 6 | J29 |  |
| GPIO41 | MDIO1/MSDA3 |  | IO_MUX_SEL_1, BIT 5 & 6 | J19 |  |
| GPIO42 | SPI-MEMORY |  | RTL8373_INI_MODE_ADDR, BIT 0 & 1 | D1 |  |
| GPIO43 | SPI-MEMORY |  | RTL8373_INI_MODE_ADDR, BIT 0 & 1 | E1 |  |
| GPIO44 | SPI-MEMORY |  | RTL8373_INI_MODE_ADDR, BIT 0 & 1 | D2 |  |
| GPIO45 | SPI-MEMORY |  | RTL8373_INI_MODE_ADDR, BIT 0 & 1 | E2 |  |
| GPIO46 | MSCK0 |  | IO_MUX_SEL_1, BIT 7 & 8 | A2 |  |
| GPIO47 | MSDA0 |  | IO_MUX_SEL_1, BIT 9 & 10 | B2 |  |
| GPIO48 | MSCK1 |  | IO_MUX_SEL_1, BIT 11 & 12 | A1 |  |
| GPIO49 | MSDA1 |  | IO_MUX_SEL_1, BIT 13 & 14 | B1 |  |
| GPIO50 | MSCL2/U1TXD |  | IO_MUX_SEL_1, BIT 15 & 16 | C1 |  |
| GPIO51 | MSDA2/U1RXD |  | IO_MUX_SEL_1, BIT 17 & 18 | C2 |  |
| GPIO52 | ACL_BIT0_EN |  | IO_MUX_SEL_2, BIT 0 |  |  |
| GPIO53 | ACL_BIT1_EN |  | IO_MUX_SEL_2, BIT 1 |  |  |
| GPIO54 | ACL_BIT2_EN |  | IO_MUX_SEL_2, BIT 2 | E5 |  |
| GPIO55 | PTP_CLK125M_IN |  | IO_MUX_SEL_1, BIT 19 |  |  |
| GPIO56 | PTP_CLK_OUT |  | IO_MUX_SEL_1, BIT 20 |  |  |
| GPIO57 | PTP_TOD_OUT |  | IO_MUX_SEL_1, BIT 21 |  |  |
| GPIO58 | PTP_PPS_OUT |  | IO_MUX_SEL_1, BIT 22 |  |  |
| GPIO59 | PTP_TOD_IN |  | IO_MUX_SEL_1, BIT 23 |  |  |
| GPIO60 | PTP_PPS_IN |  | IO_MUX_SEL_1, BIT 24 |  |  |
| GPIO61 | SYNCELOCK0 |  | IO_MUX_SEL_1, BIT 27 |  |  |
| GPIO62 | SYNCELOCK1 |  | IO_MUX_SEL_1, BIT 28 |  |  |
| GPIO63 | GPIO_MDIO0 |  | IO_MUX_SEL_1, BIT 4 |  |  |

## I2C

| I2C | Function |Type  | (RTL8372) PIN# | (RTL8372N) PIN# |
|  ---- | ---- | ---- | ---- | ---- |
| GPIO47 | SDA0 | I/OPU |  | B1 | 142 |
| GPIO49 | SDA1 | I/OPU |  | B2 | 144 |
| GPIO51 | SDA2 | I/OPU |  | C2 | ??? |
| GPIO41 | SDA3 | I/OPU |  | J20 | 98 |
| GPIO39 | SDA4 | I/OPU |  | K20 | 95 |
| GPIO46 | SCL0 | I/OPU |  | A2 | 138 |
| GPIO48 | SCL1 | I/OPU |  | A1 | 140 |
| GPIO50 | SCL2 | I/OPU |  | C1 | ??? |
| GPIO40? | SCL3 | OPU |  | J20 |  |

# Other funcitons

| Function | Type | (RTL8372) PIN# | (RTL8372N) PIN# |
|  ---- | ---- | ---- | ---- | 
| nRESET   |      | A6  | 131 |
| PTP_SYNC |      | B10 | 130 |
| INT      | OPU  | B6  | 132 |




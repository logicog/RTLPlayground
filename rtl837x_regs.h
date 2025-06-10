#ifndef _RTL837X_REGS_H_
#define _RTL837X_REGS_H_

#define RTL837X_REG_HW_CONF 0x6040
// Bits 4 & 5: CLOCK DIVIDER from 125MHz for Timer

#define RTL837X_REG_LED_MODE 0x6520
// Defines the LED Mode for steering the Port LEDS and the System LED
// BIT 17 set: LED solid on
// Bytes 0/1 hold the LED mode, e.g. serial, RTL8231?
#define RTL837X_REG_SMI_CTRL 0x6454
#define RTL837X_REG_RESET 0x0024
#define RTL837X_REG_SEC_COUNTER 0x06f4
#define RTL837X_REG_SDS_MODES 0x7b20
#define RTL837X_REG_LINKS 0x63f0

// Blink rate is defined by setAsicRegBits(0x6520,0xe00000,rate);

#endif

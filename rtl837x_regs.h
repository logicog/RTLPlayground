#ifndef _RTL837X_REGS_H_
#define _RTL837X_REGS_H_

#define RTL837X_REG_HW_CONF 0x6040
// Bits 4 & 5: CLOCK DIVIDER from 125MHz for Timer

#define RTL837X_REG_LED_MODE 0x6520
// Defines the LED Mode for steering the Port LEDS and the System LED
// BIT 17 set: LED solid on
// Bytes 0/1 hold the LED mode, e.g. serial, RTL8231?
// Blink rate is defined by setAsicRegBits(0x6520,0xe00000,rate);

#define RTL837X_REG_SMI_CTRL 0x6454
#define RTL837X_REG_RESET 0x0024
// Writing 0x01 into this register causes a reset of the entire SoC
#define RTL837X_REG_SEC_COUNTER 0x06f4
// Used for counting seconds

#define RTL837X_REG_SDS_MODES 0x7b20
/*
 * 5 Bits each give the state of the 2 SerDes of the RTL8372
 * Values are:
 */
#define SDS_SGMII		0x02
#define SDS_1000BX_FIBER	0x04
#define SDS_HISGMII		0x12
#define SDS_10GR		0x1a

#define RTL837X_REG_LINKS 0x63f0
/* Each nibble encodes the link state of a port.
   Port 0 appears to be the CPU port
   The RTL8372 serves ports 4-7, port 3 is the RTL8221
   2: 1Gbit
   5: 2.5Gbit
  */

#define RTL837X_REG_GPIOA 0x40
// BIT 4 resets RTL8224 on 9000-9XH

#define RTL837X_REG_GPIOB 0x44
// Bit 1e cleared: SFP Module inserted on 9000-6XH (MOD_DEF0 pin)

#define RTL837X_REG_GPIOC 0x48
// BIT 5 set: SIGNAL LOS of SFP module on 9000-6XH (RX_LOS pin)


#endif

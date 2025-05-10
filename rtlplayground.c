#include <8051.h>
#include <stdint.h>

#include "rtl837x_sfr.h"
#include "rtl837x_flash.h"

#define SYS_TICK_HZ 100
#define SERIAL_BAUD_RATE 57600

/* All RTL839x switches have an external 25MHz Oscillator,
   VALID RTL8372/3 CPU frequencies found in switches are:
   0x07735940 = 125,000,000
   0x03b9aca0 =  62,500,000
   0x01dcd650 =  31,250,000
   0x013d6200 =  20,800,000
   For the following frequencies, divider settings are known
   and can be selected on all known HW (Register 0x6040)
*/
#define CLOCK_HZ 125000000
// #define CLOCK_HZ 20800000

// Derive the divider settings for the internal clock
#if CLOCK_HZ == 20800000
#define CLOCK_DIV 3
#elif CLOCK_HZ == 31250000
#define CLOCK_DIV 2
#elif CLOCK_HZ == 62500000
#define CLOCK_DIV 1
#elif CLOCK_HZ == 125000000
#define CLOCK_DIV 0
#endif

#define RTL837X_REG_LED_MODE 0x6520
#define RTL837X_REG_SMI_CTRL 0x6454
#define RTL837X_REG_RESET 0x0024

// Blink rate is defined by setAsicRegBits(0x6520,0xe00000,rate);
#define SFR_EXEC_READ_REG 1
#define SFR_EXEC_WRITE_REG 3
#define SFR_EXEC_READ_SMI 9
#define SFR_EXEC_WRITE_SMI 11

__xdata unsigned short ticks;

#define N_WORDS 10
__xdata signed char cmd_words_b[N_WORDS];

// Buffer for serial input, SBUF_SIZE must be power of 2 < 256
#define SBUF_SIZE 32
__xdata char sbuf_ptr;
__xdata unsigned sbuf[SBUF_SIZE];

__code unsigned char * __code greeting = "HI! This is a minimal prompt to explore the RTL8372!\r\n";
__code unsigned char * __code hex = "0123456789abcdef";


#define N_COMMANDS 1
struct command {
	unsigned char *cmd;
	unsigned char id;
};


void isr_timer0(void) __interrupt(1)
{
	TR0 = 0;		// Stop timer 0
	TH0 = (0x10000 - (CLOCK_HZ / SYS_TICK_HZ / 32)) >> 8;
	TL0 = (0x10000 - (CLOCK_HZ / SYS_TICK_HZ / 32)) % 0xff;
	ticks++;
	
	/*
	SFR_DATA_24 = 0x00;
	SFR_DATA_16 = 0x23;
	SFR_DATA_8 = 0xe0;
	SFR_DATA_0 = 0xf0;
	SFR_REG_ADDRH = RTL837X_REG_LED_MODE >> 8;
	SFR_REG_ADDRL = RTL837X_REG_LED_MODE & 0xff;
	SFR_EXEC_GO = SFR_EXEC_WRITE_REG;
	do {
	} while (SFR_EXEC_STATUS != 0);
	*/
	TR0 = 1;		// Re-start timer 0
}


void isr_serial(void) __interrupt(4)
{
	if (RI == 1) {
		sbuf[sbuf_ptr] = SBUF;
		sbuf_ptr = (sbuf_ptr + 1) & (SBUF_SIZE - 1);
		RI = 0;
	}
}


void write_char(char c)
{
	do {
	} while (TI == 0);
	TI = 0;
	SBUF = c;
}


void print_string(__code char *p)
{
	while (*p)
		write_char(*p++);
}


void print_short(unsigned short a)
{
	print_string("0x");
	for (signed char i = 12; i >= 0; i -= 4) {
		write_char(hex[(a >> i) & 0xf]);
	}
}


void print_byte(unsigned char a)
{
	write_char(hex[(a >> 4) & 0xf]);
	write_char(hex[a & 0xf]);
	write_char(' ');
}


/*
 * External IRQ 0 Service Routine
 * Note that all registers are being put on the STACK because of calling a subroutine
 */
void isr_ext0(void) __interrupt(0)
{
	EX0 = 0;
	write_char('X');
	IT0 = 1;
	EX0 = 1;
}

/*
 * External IRQ 1 Service Routine, triggered by the NIC recieving a packet
 * Note that all registers are being put on the STACK because of calling
 * a subroutine (write_char), we shold do better...
 */
void isr_ext1(void) __interrupt(2)
{
	
	// This flag should only be reset after all packets have been read
	EX1 = 0;
	write_char('Y');
}

/*
 * External IRQ 2 Service Routine
 * Note that all registers are being put on the STACK because of calling a subroutine
 */
void isr_ext2(void) __interrupt(8)
{
	EXIF &= 0xef;	// Clear IRQ flag (bit 7) in EXIF
	write_char('Z');
	PCON |= 1; // Enter Idle mode until interrupt occurs
}

/*
 * External IRQ 3 Service Routine
 * Note that all registers are being put on the STACK because of calling a subroutine
 */
void isr_ext3(void) __interrupt(9)
{
	EXIF &= 0xdf;	// Clear IRQ flag (bit 6) in EXIF
	write_char('W');
}


void setup_timer0(void)
{
	TMOD = 0x11;  // Timer 1: Mode 1, Timer 0: Mode 1, i.e. 16 bit counters, no auto-reload
	// The TH0 registers contain the high/low byte that is loaded into
	// timer0 when T0 overflows to 0x10000
	TH0 = (0x10000 - (CLOCK_HZ / SYS_TICK_HZ / 32)) >> 8;
	TL0 = (0x10000 - (CLOCK_HZ / SYS_TICK_HZ / 32)) % 0xff;

	TCON = 0x10;	// Start timer 0
	CKCON &= 0xc7;
	ET0 = 1;	// Enable timer interrupts
}


void reg_read(uint16_t reg_addr)
{
	SFR_REG_ADDRH = reg_addr >> 8;
	SFR_REG_ADDRL = reg_addr;
	SFR_EXEC_GO = SFR_EXEC_READ_REG;
	do {
	} while (SFR_EXEC_STATUS != 0);
	/* The result is now in SFR A4, A5, A6, A7 */
}


void reg_write(uint16_t reg_addr)
{
	/* Data to write must be in SFR A4, A5, A6, A7 */
	SFR_REG_ADDRH = reg_addr >> 8;
	SFR_REG_ADDRL = reg_addr;
	SFR_EXEC_GO = SFR_EXEC_WRITE_REG;
	do {
	} while (SFR_EXEC_STATUS != 0);
}


/* This sets a bit in the 32bit wide switch register reg_addr
 */
void reg_bit_set(uint16_t reg_addr, char bit)
{
	reg_read(reg_addr);
	unsigned char bit_mask = 1 << (bit & 0x3);
	switch (bit >> 3) {
	case 0:
		SFR_DATA_0 |= bit_mask;
		break;
	case 1:
		SFR_DATA_8 |= bit_mask;
		break;
	case 2:
		SFR_DATA_16 |= bit_mask;
		break;
	case 3:
		SFR_DATA_24 |= bit_mask;
		break;
	}
	reg_write(reg_addr);
}


/* This sets a bit in the 32bit wide switch register reg_addr
 */
void reg_bit_clear(uint16_t reg_addr, char bit)
{
	reg_read(reg_addr);
	unsigned char bit_mask = 1 << (bit & 0x3);
	bit_mask = ~bit_mask;
	switch (bit >> 3) {
	case 0:
		SFR_DATA_0 &= bit_mask;
		break;
	case 1:
		SFR_DATA_8 &= bit_mask;
		break;
	case 2:
		SFR_DATA_16 &= bit_mask;
		break;
	case 3:
		SFR_DATA_24 &= bit_mask;
		break;
	}
	reg_write(reg_addr);
}


/* Read flash using the MMIO capabilities of the DW8051 core
 * Bank is < 0x3f and is the MSB
 * addr gives the address in the bank
 * Note that the address in the flash memory is not simply 0xbbaddr, because
 * the size of a bank is merely 0xc000.
 */
unsigned char read_flash(unsigned char bank, __code unsigned char *addr)
{
	unsigned char v;
	unsigned char current_bank = SFR_BANK;

	SFR_BANK = bank;
	v = *addr;
	SFR_BANK = current_bank;
	return v;
}


void reset_chip(void)
{
	SFR_DATA_24 = 0x00;
	SFR_DATA_16 = 0x00;
	SFR_DATA_8 = 0x00;
	SFR_DATA_0 = 0x01;
	reg_write(RTL837X_REG_RESET);
}


void setup_external_irqs(void)
{
	SFR_DATA_24 = 0x00;
	SFR_DATA_16 = 0x00;
	SFR_DATA_8 = 0x00;
	SFR_DATA_0 = 0x42;
	reg_write(0x5f84);

	SFR_DATA_8 = 0x03;
	SFR_DATA_0 = 0xff;
	reg_write(0x5f34);

	EX0 = 1;	// Enable external IRQ 0
	IT0 = 1;	// External IRQ on falling edge

	EX1 = 1;	// External IRQ 1 enable
	EX2 = 1;	// External IRQ 2 enable: bit EIE.0
	EX3 = 1;	// External IRQ 3 enable: bit EIE.1
	PX3 = 1;	// Set EIP.1 = 1: External IRQ 3 set to high priority
}


void reset_rtl8224(void)
{
	/* Toggle reset pin on RTL8224 on RTL8373 */
	reg_bit_clear(0x40, 4);
	reg_bit_set(0x50, 4);
	reg_bit_set(0x40, 4);
}


/*
 * Set dividers for a chosen CPU frequency
 */
void setup_clock(void)
{
	reg_read(0x6040);
	SFR_DATA_0 &= ~0x30;
	SFR_DATA_0 |= CLOCK_DIV << 4; // Divider in bits 4 & 5
	SFR_DATA_8 |= 0x01;  // This is set in managed mode 125MHz
	reg_write(0x6040);

	reg_read(0x7f90);
	SFR_DATA_0 &= 0xfd;
	SFR_DATA_0 |= 0x01;
	reg_write(0x7f90);
}


void print_sfr_data(void)
{
	write_char('0');
	write_char('x');
	write_char(hex[SFR_DATA_24 >> 4]);
	write_char(hex[SFR_DATA_24 & 0xf]);
	write_char(hex[SFR_DATA_16 >> 4]);
	write_char(hex[SFR_DATA_16 & 0xf]);
	write_char(hex[SFR_DATA_8 >> 4]);
	write_char(hex[SFR_DATA_8 & 0xf]);
	write_char(hex[SFR_DATA_0 >> 4]);
	write_char(hex[SFR_DATA_0 & 0xf]);
}


/*
 * Write a register reg of phy phy_id, in page page
 * Data to be written must be in SFR a6/a7
 */
void phy_write(unsigned char phy_mask, unsigned char dev_id, unsigned short reg, unsigned short v)
{
	SFR_DATA_8 = v >> 8;
	SFR_DATA_0 = v;
	SFR_SMI_PHYMASK = phy_mask;
	SFR_SMI_REG_H = reg >> 8;
	SFR_SMI_REG_L = reg;
	SFR_SMI_DEV = dev_id  << 3 | 2; //
	SFR_EXEC_GO = SFR_EXEC_WRITE_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);
}


void rtl8372_init(void)
{
	// From run, set bits 0-1 to 1
	print_string("rtl8372_init called\r\n");
	reg_read(0x7f90);
	SFR_DATA_0 &= 0xfc;
	SFR_DATA_0 |= 0x01;
	reg_write(0x7f90);

	reg_read(0x6330);
	SFR_DATA_0 |= 0xc0; // Set Bits 6, 7
	SFR_DATA_16 &= 0xfc; // Delete bits 16, 17
	reg_write(0x6330);
	reg_read(0x6334);	// Also in sdsMode_set
	SFR_DATA_8 |= 0x01;  	// Set bits 3-8, On RTL8373+8224 set bits 0-7
	SFR_DATA_0 |= 0xf8;
	reg_write(0x6334);

	// Enable MDC
	reg_read(RTL837X_REG_SMI_CTRL);
	SFR_DATA_8 |= 0x70; // Set bits 0xc-0xe to enable MDC for SMI0-SMI2
	reg_write(RTL837X_REG_SMI_CTRL);

/*	FUN_CODE_2023();
	FUN_CODE_01a6();
*/
	reg_read(RTL837X_REG_LED_MODE);
//	SFR_DATA_24 = 0x00;
	SFR_DATA_16 &= 0x1f; // Mask blink rate field (0xe0)
	SFR_DATA_16 |= 0x23; // Set blink rate and LED to solid (set bit 1 = bit 17 overall)

	// Configure led-mode (serial?)
	SFR_DATA_8 &= 0xe3; // 0xe0;
	SFR_DATA_8 |= 0x0c;
	SFR_DATA_0 &= 0x1f; // 0xf0;
	SFR_DATA_0 |= 0xe0;
	reg_write(RTL837X_REG_LED_MODE);
/*
	BEFORE: 0x0021fdb0
	AFTER:  0x0021fdf0
	CORRECT:0x0023e0f0;

	rVar1 = rtl8373_setAsicRegBits(0x6520,0xe0 0000,rate);
	write_xmem_32((uint *)0xb1,0xc);
	write_xmem_32((uint *)0xb5,9);
	write_xmem_32((uint *)0xb9,3); 
	rtl8373_setAsicRegBits_call_b1(0x6520);
	write_xmem_32((uint *)0xb1,8);
	write_xmem_32((uint *)0xb5,5);
	write_xmem_32((uint *)0xb9,5);
	rtl8373_setAsicRegBits_call_b1(0x6520);  // LED Enable
*/

	// Clear bits 0,1 of 0x65f8
	reg_read(0x65f8);
	SFR_DATA_0 &= 0xfc;
	reg_write(0x65f8);

	// Set 0x65fc to 0xfffff000
	SFR_DATA_24 = 0xff;
	SFR_DATA_16 = 0xff;
	SFR_DATA_8 = 0xf0;
	SFR_DATA_0 = 0x00;
	reg_write(0x65fc);

	// Set bits 0-3 of 0x6600 to 0xf
	reg_read(0x6600);
	SFR_DATA_0 |= 0x0f;
	reg_write(0x6600);

	// Set bit 0x1d of 0x65dc, clear bit 1b
	reg_bit_set(0x65dc, 0x1d);
	reg_bit_clear(0x65dc, 0x1b);

	// Set bits 1b/1d of 0x7f8c
	reg_bit_set(0x7f8c, 0x1d);
	reg_bit_set(0x7f8c, 0x1b);

	// Configure LED_SET_0, ledid 0/1
	SFR_DATA_24 = 0x00;
	SFR_DATA_16 = 0x41;
	SFR_DATA_8 = 0x01;
	SFR_DATA_0 = 0x75;
	reg_write(0x6548);

	// Configure LED_SET_0 ledid 2
	reg_read(0x6544);
	SFR_DATA_8 = 0x00;
	SFR_DATA_0 = 0x44;
	reg_write(0x6544);

	// Further configure LED_SET_0
	reg_read(0x6528);
	SFR_DATA_0 = 0x11;
	reg_write(0x6528);

	// Part of the SDS configuration, see sdsMode_set, set bits 0xa-0xe to 0
	reg_read(0x6450);
	SFR_DATA_8 &= 0x83;
	reg_write(0x6450);

	// SDS bits 10-1f set to 0
	reg_read(0x644c);
	SFR_DATA_8 &= 0xe0;
	reg_write(0x644c);
/*
	FUN_CODE_018d(0,1,0,8);
	FUN_CODE_018d(0,1,0,3);
*/

	// Set the SerDes mode. Bits 0-4: SDS 0, Bits 5-9: SDS 1. Bits set to 1f
	reg_read(0x7b20);
	SFR_DATA_8 |= 0x03;
	SFR_DATA_0 = 0xff;
	reg_write(0x7b20);
/*	
	calll_4464_bank1();
	calll_4464_bank1();
	calll_4464_bank1();
*/

	/* FUN_CODE_2023();
	phy_setting_up_somehow();
	FUN_CODE_2023();
	phy_setting_up_somehow();
	*/

	reg_read(0xa90);
	SFR_DATA_0 &= 0xf0;
	SFR_DATA_0 |= 0xc;
	reg_write(0xa90);

	// Disable PHYs for configuration
	phy_write(0xf0,0x1f,0xa610,0x2858);

	// Set bits 0x13 and 0x14 of 0x5fd4
	reg_bit_set(0x5fd4, 0x13);
	reg_bit_set(0x5fd4, 0x14);

	// Configure ports 3-8:
	uint16_t reg = 0x1238 + 0x300; // Port base register for the bits we set
	for (char i = 0; i < 6; i++) {
		reg_bit_set(reg, 0x2);
		reg_bit_set(reg, 0x8);
		reg_bit_set(reg, 0x8);
		reg += 0x100;
	}

	reg_bit_set(0xb7c, 5);
	reg_bit_set(0x6040, 0);

/*
	FUN_CODE_2023();
	uVar4 = 1;
	FUN_CODE_0617(0x23f);
	bVar7 = eql_l(a,uVar4);
	if (bVar7 == 0) {
		FUN_CODE_019c();
		FUN_CODE_01a1();
	}
	else {
		uVar4 = 2;
		FUN_CODE_0617(0x23f);
		bVar7 = eql_l(a,uVar4);
		if (bVar7 == 0) {
			FUN_CODE_0192();
			FUN_CODE_0197();
		}
	}
*/

/*
	FUN_CODE_01bf();
	FUN_CODE_01ab();
	FUN_CODE_01b0();
	FUN_CODE_01ba();
	FUN_CODE_01b5();
*/
	// Re-enable PHY after configuration
	phy_write(0xf0,0x1f,0xa610,0x2058);;

	// Set bits 0xc-0x14 of 0x632c to 0x1f8, see rtl8372_init
	reg_read(0x632c);
	SFR_DATA_8 &= 0x8f;
	SFR_DATA_8 |= 0x80;
	SFR_DATA_16 &= 0xe0;
	SFR_DATA_16 |= 0x1f;
	reg_write(0x632c);
}


/*
 * Read a phy register via MDIO clause 45
 * Input must be: phy_id < 64,  device_id < 32,  reg < 0x10000)
 * The result is in SFR A6 and A7 (SFR_DATA_8, SFR_DATA_0)
 */
void phy_read(unsigned char phy_id, unsigned char device, unsigned short reg)
{
	SFR_SMI_PHY = phy_id;
	SFR_SMI_REG_H = reg >> 8;
	SFR_SMI_REG_L = reg;
	SFR_SMI_DEV = device << 3 | 2;
	SFR_EXEC_GO = SFR_EXEC_READ_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);
}



void led_enable(void)
{
	reg_read(RTL837X_REG_LED_MODE);
	SFR_DATA_24 = 0x00;
	SFR_DATA_16 = 0x23;
	SFR_DATA_8 = 0xe0;
	SFR_DATA_0 = 0xf0;
//	SFR_DATA_0 &= 0xe0;
//	SFR_DATA_8 &= 0x1f;
//	SFR_DATA_0 |= 0xe0;
//	SFR_DATA_8 |= 0x06;
	reg_write(RTL837X_REG_LED_MODE);
}

void led_disable(void)
{
	SFR_DATA_24 = 0x00;
	SFR_DATA_16 = 0x00;
	SFR_DATA_8 = 0x00;
	SFR_DATA_0 = 0x00;
	reg_write(RTL837X_REG_LED_MODE);
}



void port_leds_on(void)
{
	reg_read(0x6528);
	SFR_DATA_0 = 0x11;
	reg_write(0x6528);
}


/* Set up serial port 0 using Timer 2 with an external trigger
 * as baud generator.
 * The external clock generator uses a crystal at 25MHz.
 */
void setup_serial(void)
{
	IE = 0;

	T2CON = 0x34; // Enable RCLK/TCLK (serial transmit/receive clock for T2), TR2 (Timer 2 RUN), disable CP/RL2 (bit 0)
	SCON = 0x50;  // Mode = 1: ASYNC 8N1 with T2 as baud-rate generator, REN_0 Receive enable

	// The RCAP2 registers contain the high/low byte that is loaded into
	// timer2 when T2 overflows to 0x10000
	RCAP2H = (0x10000 - (CLOCK_HZ / SERIAL_BAUD_RATE / 32)) >> 8;
	RCAP2L = (0x10000 - (CLOCK_HZ / SERIAL_BAUD_RATE / 32)) % 0xff;

	PCON |= 0x80; // Double the Baud Rate

	SCON = 0x50;
	TI = 1;
	RI = 0;

	ES = 1; // Enable serial IRQ
}


void print_reg(unsigned short reg)
{
	reg_read(reg);
	print_sfr_data();
}


void print_phy_reg(unsigned char phy_id, unsigned char device, unsigned short reg)
{
	phy_read(phy_id, device, reg);
	SFR_DATA_16 = SFR_DATA_24 = 0;
	print_sfr_data();
}


__code struct command commands[N_COMMANDS] = {
	{ "reset", 1 },
};


unsigned char cmd_compare(unsigned char start, unsigned char * __code cmd)
{
	signed char i;
	signed char j = 0;

	for (i = cmd_words_b[start]; i < cmd_words_b[start + 1] && sbuf[i] != ' '; i++) {
		if (!cmd[j])
			return 1;
		if (sbuf[i++] != cmd[j++])
			break;
	}
	if (i >= cmd_words_b[start + 1] || sbuf[i] == ' ')
		return 1;
	return 0;
}


void bootloader(void)
{
	ticks = 0;
	sbuf_ptr = 0;

	CKCON = 0;
	SFR_97 = 0;

	// Set in managed mode:
	SFR_b9 = 0x00;
	SFR_ba = 0x80;

	// Disable all interrupts (global and individually) by setting IE register (SFR A8) to 0
	IE = 0;
	EIE = 0;  // SFR e8: EIE. Disable all external IRQs

	// Disable all interrupts (global interrupt enable bit)
	EA = 0; // SFR A8.7 / IE.7

	// HW setup, serial, timer, external IRQs
	setup_clock();
	setup_serial();
	setup_timer0();
	setup_external_irqs();
	EA = 1; // Enable all IRQs

// 	port_leds_on();
	print_string("\r\nStarting up...\r\n");
	print_string("  Flash controller\r\n");
	flash_init();
	print_string("  > OK\r\n current status: ");
	print_short(flash_read_status());

	print_string("\r\n  READ JEDEC-ID\r\n");
	flash_read_jedecid();
	print_string("\r\n  Testing read UID\r\n");
	flash_read_uid();
	print_string("\r\n  Testing read Securty Register 1\r\n");
	flash_read_security(0x0001000, 40);
	print_string("\r\n  Testing read Securty Register 2\r\n");
	flash_read_security(0x0002000, 40);
	print_string("\r\n  Testing read Securty Register 3\r\n");
	flash_read_security(0x0003000, 40);
//	flash_read_uid();
//	flash_write_enable();

	print_string("  > status: ");
	print_short(flash_read_status());
	print_string("\r\n  Dumping flash at 0x0\r\n");
	flash_dump(0, 252);
	print_string("\r\n  Dumping flash at 0x100\r\n");
	flash_dump(0x100, 252);
	print_string("\r\nREADING PHY 0x1, 0x4, 0: ");
	print_phy_reg(0x1, 0x4, 0);
	rtl8372_init();

	print_string(greeting);
	print_string("READING PHY 0x4, 0x1, 0: ");
	print_phy_reg(0x4, 0x1, 0);
	print_string("\r\nREADING PHY 0x1, 0x4, 0: ");
	print_phy_reg(0x1, 0x4, 0);
	print_string("\r\nREADING PHY 0x5, 0x1, 1: ");
	print_phy_reg(0x5, 0x1, 1);
	print_string("\r\nREADING PHY 0x1, 0x1, 0: ");
	print_phy_reg(0x1, 0x1, 0);
	print_string("\r\nREADING PHY 0x7, 0x1f, 0xa412:");
	print_phy_reg(0x7, 0x1f, 0xa412);
	print_string("\r\nCPU version: ");
	print_reg(0x4);

	
	print_string("\r\n> ");
	char l = sbuf_ptr;
	char line_ptr = l;
	char is_white = 1;
	while (1) {
		while (l != sbuf_ptr) {
			write_char(sbuf[l]);
			// Check whether there is a full line:
			if (sbuf[l] == '\n' || sbuf[l] == '\r') {
				print_short(ticks);
				// Print line and parse command into words
				print_string("\r\n  CMD: ");
				is_white = 1;
				unsigned char word = 0;
				cmd_words_b[0] = -1;
				while (line_ptr != l) {
					if (is_white && sbuf[line_ptr] != ' ') {
						is_white = 0;
						cmd_words_b[word++] = line_ptr;
					}
					if (sbuf[line_ptr] == ' ')
						is_white = 1;
					write_char(sbuf[line_ptr++]);
					line_ptr &= SBUF_SIZE - 1;
				}
				cmd_words_b[word++] = line_ptr;
				cmd_words_b[word++] = -1;
				line_ptr = (l + 1) & (SBUF_SIZE - 1);

				// Identify command
				signed char i = cmd_words_b[0];
				if (i >= 0) {
					print_string("\r\n  THERE may be a command: ");
					print_short(i); print_string("\r\n");
					print_short(cmd_words_b[0]); print_string("\r\n");
					print_short(cmd_words_b[1]); print_string("\r\n");
					print_short(cmd_words_b[2]); print_string("\r\n");
					print_short(cmd_words_b[3]); print_string("\r\n");
				}
				
				if (cmd_compare(0, "reset")
					reset_chip();
			}
			l++;
			l &= (SBUF_SIZE - 1);
		}
		PCON |= 1; // Enter Idle mode until interrupt occurs
	}
}

#include <8051.h>
#include <stdint.h>

#include "rtl837x_sfr.h"
#include "rtl837x_flash.h"

#include "rtl837x_regs.h"
#define SYS_TICK_HZ 100
#define SERIAL_BAUD_RATE 115200

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
//#define CLOCK_HZ 20800000

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

__xdata uint8_t isRTL8373;

volatile __xdata uint32_t ticks;
volatile __xdata uint8_t sec_counter;
__xdata uint32_t sleep_until;

#define N_WORDS 10
__xdata signed char cmd_words_b[N_WORDS];

// Buffer for serial input, SBUF_SIZE must be power of 2 < 256
#define SBUF_SIZE 32
__xdata char sbuf_ptr;
__xdata uint8_t sbuf[SBUF_SIZE];
__xdata uint8_t sfr_data[4];

__code uint8_t * __code greeting = "A minimal prompt to explore the RTL8372!\r\n";
__code uint8_t * __code hex = "0123456789abcdef";

__xdata uint8_t flash_buf[256];

__code uint16_t bit_mask[16] = {
	0x0001, 0x0002, 0x0004,0x0008,0x0010,0x0020,0x0040, 0x0080,
	0x0100, 0x0200, 0x0400,0x0800,0x1000,0x2000,0x4000, 0x8000
};

__xdata uint8_t linkbits_last[4];
__xdata uint8_t sfp_pins_last;

#define N_COMMANDS 1
struct command {
	uint8_t *cmd;
	uint8_t id;
};


void isr_timer0(void) __interrupt(1)
{
	TR0 = 0;		// Stop timer 0
	TH0 = (0x10000 - (CLOCK_HZ / SYS_TICK_HZ / 32)) >> 8;
	TL0 = (0x10000 - (CLOCK_HZ / SYS_TICK_HZ / 32)) % 0xff;

	ticks++;
	sec_counter++;

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

void print_short(uint16_t a)
{
	print_string("0x");
	for (signed char i = 12; i >= 0; i -= 4) {
		write_char(hex[(a >> i) & 0xf]);
	}
}


void print_long(uint32_t a)
{
	print_string("0x");
	for (signed char i = 28; i >= 0; i -= 4) {
		write_char(hex[(a >> i) & 0xf]);
	}
}

void print_byte(uint8_t a)
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
	EX0 = 0;	// Disable interrupt for the moment
	write_char('X');
	IT0 = 1;	// Trigger on falling edge of external interrupt
	EX0 = 1;	// Re-enable interrupt
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
	EX1 = 1;
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


void reg_read_m(uint16_t reg_addr)
{
	SFR_REG_ADDRH = reg_addr >> 8;
	SFR_REG_ADDRL = reg_addr;
	SFR_EXEC_GO = SFR_EXEC_READ_REG;
	do {
	} while (SFR_EXEC_STATUS != 0);
	sfr_data[0] = SFR_DATA_24;
	sfr_data[1] = SFR_DATA_16;
	sfr_data[2] = SFR_DATA_8;
	sfr_data[3] = SFR_DATA_0;
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


void reg_write_m(uint16_t reg_addr)
{
	SFR_REG_ADDRH = reg_addr >> 8;
	SFR_REG_ADDRL = reg_addr;
	SFR_DATA_24 = sfr_data[0] ;
	SFR_DATA_16 = sfr_data[1];
	SFR_DATA_8 = sfr_data[2];
	SFR_DATA_0 = sfr_data[3];

	SFR_EXEC_GO = SFR_EXEC_WRITE_REG;
	do {
	} while (SFR_EXEC_STATUS != 0);
}


/*
 * This sets a bit in the 32bit wide switch register reg_addr
 */
void reg_bit_set(uint16_t reg_addr, char bit)
{
	uint8_t bit_mask = 1 << (bit & 0x7);

	bit >>= 3;
	reg_read_m(reg_addr);
	sfr_data[3-bit] |= bit_mask;
	reg_write_m(reg_addr);
}


/*
 * This sets a bit in the 32bit wide switch register reg_addr
 */
void reg_bit_clear(uint16_t reg_addr, char bit)
{
	uint8_t bit_mask = 1 << (bit & 0x7);

	bit >>= 3;
	reg_read_m(reg_addr);
	bit_mask = ~bit_mask;
	sfr_data[3-bit] &= bit_mask;
	reg_write_m(reg_addr);
}

/*
 * This masks the sfr data fields, first &-ing with ~mask, the setting the bits in set
 */
void sfr_mask_data(uint8_t n, uint8_t mask, uint8_t set)
{
	uint8_t b = sfr_data[3-n];
	b &= ~mask;
	b |= set;
	sfr_data[3-n] = b;
}


/* Read flash using the MMIO capabilities of the DW8051 core
 * Bank is < 0x3f and is the MSB
 * addr gives the address in the bank
 * Note that the address in the flash memory is not simply 0xbbaddr, because
 * the size of a bank is merely 0xc000.
 */
uint8_t read_flash(uint8_t bank, __code uint8_t *addr)
{
	uint8_t v;
	uint8_t current_bank = SFR_BANK;

	SFR_BANK = bank;
	v = *addr;
	SFR_BANK = current_bank;
	return v;
}


void print_long_x(__xdata uint8_t v[])
{
	write_char('0'); write_char('x');
	for (int i=0; i < 4; i++) {
		write_char(hex[v[i] >> 4]);
		write_char(hex[v[i] & 0xf]);
	}
}

/*
 * Read a SerDes register in the SoC
 * Input must be: sds_id = 0/1, page < 128,  reg <= 0xff
 * The result is in SFR A6 and A7 (SFR_DATA_8, SFR_DATA_0)
 */
void sds_read(uint8_t sds_id, uint8_t page, uint8_t reg)
{
	SFR_93 = reg;			// 93
	SFR_94 = page << 1 | sds_id;	// 94
	SFR_EXEC_GO = SFR_EXEC_READ_SDS;
	do {
	} while (SFR_EXEC_STATUS != 0);
}


/*
 * Write a SerDes register in the SoC
 * Input must be: sds_id = 0/1, page < 128,  reg <= 0xff
 * The value written must be in SFR A6 and A7 (SFR_DATA_8, SFR_DATA_0)
 */
void sds_write(uint8_t sds_id, uint8_t page, uint8_t reg)
{
	SFR_93 = reg;
	SFR_94 = page << 1 | sds_id;
	SFR_EXEC_GO = SFR_EXEC_WRITE_SDS;
	do {
	} while (SFR_EXEC_STATUS != 0);
}


void sds_write_v(uint8_t sds_id, uint8_t page, uint8_t reg, uint16_t v)
{
	SFR_DATA_8 = v >> 8;
	SFR_DATA_0 = v;
	SFR_93 = reg;
	SFR_94 = page << 1 | sds_id;
	SFR_EXEC_GO = SFR_EXEC_WRITE_SDS;
	do {
	} while (SFR_EXEC_STATUS != 0);
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


void print_phy_data(void)
{
	write_char('0');
	write_char('x');
	write_char(hex[SFR_DATA_8 >> 4]);
	write_char(hex[SFR_DATA_8 & 0xf]);
	write_char(hex[SFR_DATA_0 >> 4]);
	write_char(hex[SFR_DATA_0 & 0xf]);
}


void print_reg(uint16_t reg)
{
	reg_read(reg);
	print_sfr_data();
}


void print_sds_reg(uint8_t sds_id, uint8_t page, uint8_t reg)
{
	sds_read(sds_id, page, reg);
	print_phy_data();
}


char cmp_4(__xdata uint8_t a[], __xdata uint8_t b[])
{
	for (int i = 0; i < 4; i++) {
		if (a[i] == b[i])
			continue;
		if (a[i] < b[i])
			return -1;
		else
			return 1;
	}
	return 0;
}

void cpy_4(__xdata uint8_t dest[], __xdata uint8_t source[])
{
	for (int i = 0; i < 4; i++)
		dest[i] = source[i];
}


void sds_config(uint8_t sds, uint8_t mode)
{
	print_string("\r\nsds_config: sds: ");
	print_byte(sds);
	print_string(", mode: 0x");
	print_byte(mode);
	print_string("\r\nBEFORE RTL837X_REG_SDS_MODES: ");
	print_reg(RTL837X_REG_SDS_MODES);
	reg_read_m(RTL837X_REG_SDS_MODES);
	sfr_data[0] = 0;
	sfr_data[1] = 0;
	if (!sds) {
		sfr_mask_data(0, 0x1f, mode);
	} else {
		sfr_mask_data(0, 0xe0, mode << 5);
		sfr_mask_data(1, 0xf3, mode >> 3);
	}
	reg_write_m(RTL837X_REG_SDS_MODES);
	print_string("\r\nRTL837X_REG_SDS_MODES: ");
	print_reg(RTL837X_REG_SDS_MODES);

	if (mode == SDS_10GR) // 10G Fiber
		sds_write_v(sds, 0x21, 0x10, 0x4480); // Q002110:6480
	else
		sds_write_v(sds, 0x21, 0x10, 0x6480); // Q002110:6480
	sds_write_v(sds, 0x21, 0x13, 0x0400); // Q002113:0400
	sds_write_v(sds, 0x21, 0x18, 0x6d02); // Q002118:6d02
	sds_write_v(sds, 0x21, 0x1b, 0x424e); // Q00211b:424e
	sds_write_v(sds, 0x21, 0x1d, 0x0002); // 00211d:0002
	sds_write_v(sds, 0x36, 0x1c, 0x1390); // Q00361c:1390
	sds_write_v(sds, 0x36, 0x14, 0x003f); // Q003614:003f

	uint8_t page = 0;
	SFR_DATA_0 = 0x00;
	print_string("\r\nTrying to set SDS mode to 0x");
	print_byte(mode);
	print_string("\r\n");

	switch (mode) {
	case SDS_SGMII:
	case SDS_1000BX_FIBER:
		SFR_DATA_8 = 0x03;
		page = 0x24;
		break;
	case SDS_HISGMII:
	case SDS_HSG:
		SFR_DATA_8 = 0x02;
		page = 0x28;
		break;
	case SDS_10GR:
		SFR_DATA_8 = 0x02;
		page = 0x2e;
		break;
	default:
		print_string("Error in SDS Mode\r\n");
		return;
	}
	sds_write(sds, 0x36, 0x10); // Q003610:0200

	if (page == 0x2e) {  // 10G Fiber
		sds_write_v(sds, page, 0x04, 0x0080); // Q012e04:0080
		sds_write_v(sds, page, 0x06, 0x0408); // Q012e06:0408
		sds_write_v(sds, page, 0x07, 0x020d); // Q012e07:020d
		sds_write_v(sds, page, 0x09, 0x0601); // Q012e09:0601
		sds_write_v(sds, page, 0x0b, 0x222c); // Q012e0b:222c
		sds_write_v(sds, page, 0x0c, 0xa217); // Q012e0c:a217
		sds_write_v(sds, page, 0x0d, 0xfe40); // Q012e0d:fe40
		sds_write_v(sds, page, 0x15, 0xf5c1); // Q012e15:f5c1
	} else {
		sds_write_v(sds, page, 0x04, 0x0080); // Q002804:0080
		sds_write_v(sds, page, 0x07, 0x1201); // Q002807:1201
		sds_write_v(sds, page, 0x09, 0x0601); // Q002809:0601
		sds_write_v(sds, page, 0x0b, 0x232c); // Q00280b:232c
		sds_write_v(sds, page, 0x0c, 0x9217); // Q00280c:9217
		sds_write_v(sds, page, 0x0f, 0x5b50); // Q00280f:5b50
		sds_write_v(sds, page, 0x15, 0xe7f1); // Q002815:e7f1
	}

	sds_write_v(sds, page, 0x16, 0x0443); // Q002816:0443 / Q012e16:0443
	sds_write_v(sds, page, 0x1d, 0xabb0); // Q00281d:abb0 / Q012e1d:abb0

	sds_write_v(sds, 0x06, 0x12, 0x5078); // Q000612:5078
	sds_write_v(sds, 0x07, 0x06, 0x9401); // Q000706:9401
	sds_write_v(sds, 0x07, 0x08, 0x9401); // Q000708:9401
	sds_write_v(sds, 0x07, 0x0a, 0x9401); // Q00070a:9401
	sds_write_v(sds, 0x07, 0x0c, 0x9401); // Q00070c:9401
	sds_write_v(sds, 0x1f, 0x0b, 0x0003); // Q001f0b:0003
	sds_write_v(sds, 0x06, 0x03, 0xc45c); // Q000603:c45c
	sds_write_v(sds, 0x06, 0x1f, 0x2100); // Q00061f:2100
}


/*
 * Read a register of the EEPROM via I2C
 */
uint8_t sfp_read_reg(uint8_t reg)
{
	reg_read_m(0x0418);
	sfr_mask_data(1, 0xf0, 0x70);
	reg_write_m(0x0418);

	SFR_DATA_24 = SFR_DATA_16 = SFR_DATA_8 = 0;
	SFR_DATA_0 = reg;
	reg_write(0x0420);

	// Execute I2C Read
	reg_bit_set(0x418, 0);

	// Wait for execution to finish
	do {
		reg_read_m(0x418);
	} while (sfr_data[3] & 0x1);

	reg_read_m(0x0424);
	return sfr_data[3];
}

// Sleep the given number of ticks without doing housekeeping
void delay(uint16_t t)
{
	sleep_until = ticks + t;
	while (sleep_until >= ticks)
		PCON |= 1;
}


//
// An idle function that sleeps for 1 tick and does all the house-keeping
//
void idle(void)
{
	PCON |= 1;
	if (sec_counter >= 60) {
		sec_counter -= 60;
		reg_read_m(RTL837X_REG_SEC_COUNTER);
		uint8_t v = sfr_data[3];
		v++;
		sfr_data[3] = v;
		if (!v) {
			v = sfr_data[2];
			v++;
			sfr_data[2] = v;
			if (!v) {
				v = sfr_data[1];
				v++;
				sfr_data[1] = v;
				if (!v) {
					v = sfr_data[0];
					v++;
					sfr_data[0] = v;
				}
			}
		}
		reg_write_m(RTL837X_REG_SEC_COUNTER);
	}

	reg_read_m(RTL837X_REG_LINKS);
	if (cmp_4(sfr_data, linkbits_last)) {
		print_string("\r\n<new link: ");
		print_long_x(sfr_data);
		print_string(", was ");
		print_long_x(linkbits_last);
		print_string(">\r\n");
		uint8_t p5 = sfr_data[2] >> 4;
		uint8_t p5_last = linkbits_last[2] >> 4;
		cpy_4(linkbits_last, sfr_data);
		if (p5_last != p5) {
			if (p5 == 0x5) // 2.5GBit Mode
				sds_config(0, SDS_HISGMII);
			else if (p5 == 0x2) // 1GBit
				sds_config(0, SDS_SGMII);
		}
	}

	reg_read_m(RTL837X_REG_GPIOB);
	if ((sfp_pins_last & 0x1) && (!(sfr_data[0] & 0x40))) {
		sfp_pins_last &= ~0x01;
		print_string("\r\n<MODULE INSERTED> ");
		// Read Reg 11: Encoding, see SFF-8472 and SFF-8024
		// Read Reg 12: Signalling rate (including overhead) in 100Mbit: 0xd: 1Gbit, 0x67:10Gbit
		delay(100); // Delay, because some modules need time to wake up
		uint8_t rate = sfp_read_reg(12);
		print_string("\r\nRate: "); print_byte(rate);  // Normally 1, but 0 for DAC, can be ignored?
		print_string("  Encoding: "); print_byte(sfp_read_reg(11));
		print_string("\r\n");
		for (uint8_t i = 20; i < 60; i++) {
			uint8_t c = sfp_read_reg(i);
			if (c)
				write_char(c);
		}
		print_string("\r\n");
		if (rate == 0xd)
			sds_config(1, SDS_1000BX_FIBER);
		if (rate == 0x1f)  // Ethernet 2.5 GBit
			sds_config(1, SDS_HSG);
		if (rate > 0x65 && rate < 0x70)
			sds_config(1, SDS_10GR);

	}
	if ((!(sfp_pins_last & 0x1)) && (sfr_data[0] & 0x40)) {
		sfp_pins_last |= 0x01;
		print_string("\r\n<MODULE REMOVED>\r\n");
	}

	reg_read_m(RTL837X_REG_GPIOC);
	if ((sfp_pins_last & 0x2) && (!(sfr_data[3] & 0x20))) {
		sfp_pins_last &= ~0x02;
		print_string("\r\n<RX OK>\r\n");
	}
	if ((!(sfp_pins_last & 0x2)) && (sfr_data[3] & 0x20)) {
		sfp_pins_last |= 0x02;
		print_string("\r\n<RX LOS>\r\n");
	}
}


// Sleep the given number of ticks
void sleep(uint16_t t)
{
	sleep_until = ticks + t;
	while (sleep_until >= ticks)
		idle();
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
	reg_bit_clear(RTL837X_REG_GPIOA, 4);
	reg_bit_set(0x50, 4);	// Probably also a GPIOs
	reg_bit_set(RTL837X_REG_GPIOA, 4);
}


/*
 * Set dividers for a chosen CPU frequency
 */
void setup_clock(void)
{
	reg_read_m(RTL837X_REG_HW_CONF);
	sfr_mask_data(0, 0x30, 0);
#if CLOCK_DIV != 0
	 // Divider in bits 4 & 5
	sfr_mask_data(0, 0, CLOCK_DIV << 4);
#endif
	// This is set in managed mode 125MHz
	sfr_mask_data(1, 0, 0x01);
	reg_write_m(RTL837X_REG_HW_CONF);

	reg_read_m(0x7f90);
	sfr_mask_data(0, 0x1, 0x1);
	reg_write_m(0x7f90);
}


/*
 * Write a register reg of phy phy_id, in page page
 * Data to be written must be in SFR a6/a7
 */
void phy_write(uint16_t phy_mask, uint8_t dev_id, uint16_t reg, uint16_t v)
{
	SFR_DATA_8 = v >> 8;
	SFR_DATA_0 = v;
	SFR_SMI_PHYMASK = phy_mask;
	SFR_SMI_REG_H = reg >> 8;
	SFR_SMI_REG_L = reg;
	SFR_SMI_DEV = (phy_mask >> 8) | dev_id  << 3 | 2; // bit 2 can also be set for some option
	SFR_EXEC_GO = SFR_EXEC_WRITE_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);
}


/*
 * Read a phy register via MDIO clause 45
 * Input must be: phy_id < 64,  device_id < 32,  reg < 0x10000)
 * The result is in SFR A6 and A7 (SFR_DATA_8, SFR_DATA_0)
 */
void phy_read(uint8_t phy_id, uint8_t device, uint16_t reg)
{
	SFR_SMI_REG_H = reg >> 8;	// c3
	SFR_SMI_REG_L = reg;		// c2
	SFR_SMI_PHY = phy_id;		// a5
	SFR_SMI_DEV = device << 3 | 2;	// c4

	SFR_EXEC_GO = SFR_EXEC_READ_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);
}


void nic_setup(void)
{
	// r0024:00000f80 R0024-00000f84 r0024:00000f80
	// Reset NIC
	reg_bit_set(0x24, 2);
	print_string("\r\nnic_setup");
	do {
		reg_read(0x24);
	} while (SFR_DATA_0 & 0x4);
	print_string("\r\nNIC reset");

	// Enable NIC
	// r6040:00000100 R6040-00001100
	reg_bit_set(RTL837X_REG_HW_CONF, 0xc);

	print_string("\r\nReg 0x6040: ");
	print_reg(RTL837X_REG_HW_CONF);

	// Buffer settings?
	// R7848-000004ff
	REG_SET(0x7848, 0x4ff);
	print_string("\r\nReg 0x7848: ");
	print_reg(0x7848);

	// R7844-000007fe
	REG_SET(0x7844, 0x7fe);
	print_string("\r\nReg 0x7844: ");
	print_reg(0x7844);

	// r785c:0401201e R785c-0401201e r785c:0401201e R785c-0400201e
	// 0x785c: Set bits 24-31 to 0x4, clear bits 16/17:
	print_string("\r\nB Reg 0x785c: ");
	print_reg(0x785c);
	reg_read_m(0x785c);
	sfr_mask_data(3, 0xff, 0x04);
	sfr_mask_data(2, 0x03, 0);
	reg_write_m(0x785c);
	print_string("\r\nA Reg 0x785c: ");
	print_reg(0x785c);

	// Set bit 0 of 0x7860:
	// r7860:00000000 R7860-00000001
	reg_bit_set(0x7860, 0);
	print_string("\r\nA Reg 0x7860: ");
	print_reg(0x7860);

	// r785c:0400201e R785c-0400201f
	reg_bit_set(0x785c, 0);

	// r785c:0400201f R785c-0400201b
	reg_bit_clear(0x785c, 2);
	print_string("\r\nA Reg 0x785c: ");
	print_reg(0x785c);

	// R603c-00000200
	REG_SET(0x603c, 0x200);

	// r6720:00000500 R6720-00000501 R6720-00000501 r6720:00000501
	reg_read_m(0x6720);
	sfr_mask_data(0, 1, 1);
	sfr_mask_data(1, 3, 0);
	reg_write_m(0x6720);
	print_string("\r\nA Reg 0x6720: ");
	print_reg(0x6720);

	// r6368:00000194 R6368-00000197
	reg_read_m(0x6368);
	sfr_mask_data(0, 0, 3);
	reg_write_m(0x6368);
	print_string("\r\nA Reg 0x6368: ");
	print_reg(0x6368);
}


void sds_init(void)
{
/*
	p001e.000d:9535
	R02f8-00009535 R02f4-0000953a
	P000001.1e00000d:953a
	p001e.000d:953a p001e.000d:953a
	R02f8-0000953a R02f4-00009530
	P000001.1e00000d:9530

	RTL8373:
	p001e.000d:0010
	setup_cpu in get_chip_version
	R02f8-00000010 R02f4-0000001a
	P000001.1e00000d:b7fe
	2nd call to setup_cpu in get_chip_version
	p001e.000d:0010 p001e.000d:0010
	R02f8-00000010 R02f4-00000010
	P000001.1e00000d:b7fe
*/

	print_string(", phy-reg read: ");
	phy_read(0, 0x1e, 0xd);
	uint16_t pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0;
	print_short(pval);
	print_string("\r\n");

	// PHY Initialization:
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = pval >> 8;
	SFR_DATA_0 = pval;
	reg_write(0x2f8);
	print_string("\r\nA Reg 0x2f8: ");
	print_reg(0x2f8);

	sleep(10);

	pval &= 0xfff0;
	pval |= 0x0a;
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = pval >> 8;
	SFR_DATA_0 = pval;
	reg_write(0x2f4);
	print_string("\r\nA Reg 0x2f4: ");
	print_reg(0x2f4);

	phy_write(0x1, 0x1e, 0xd, pval);

	print_string("\r\n   2: phy-reg read: ");
	phy_read(0, 0x1e, 0xd);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0;
	print_short(pval);
	print_string("\r\n");

	// PHY Initialization:
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = pval >> 8;
	SFR_DATA_0 = pval;
	reg_write(0x2f8);
	print_string("\r\nA Reg 0x2f8: ");
	print_reg(0x2f8);

	sleep(10);

	pval &= 0xfff0;
	SFR_DATA_24 = 0;
	SFR_DATA_16 = 0;
	SFR_DATA_8 = pval >> 8;
	SFR_DATA_0 = pval;
	reg_write(0x2f4);
	print_string("\r\nA Reg 0x2f4: ");
	print_reg(0x2f4);

	phy_write(0x1, 0x1e, 0xd, pval);

	if (isRTL8373) {
		reg_read_m(RTL837X_REG_SDS_MODES);
		sfr_mask_data(1, 0xfc, 0x04);
		sfr_mask_data(0, 0x1f, 0xd);
		reg_write_m(RTL837X_REG_SDS_MODES);
		print_string("\r\nA Reg SDS_MODES 0x7b20: ");
		print_reg(0x7b20);
		// q000601:c800 Q000601:c804
		// q000601:c804 Q000601:c800
		sds_read(0, 6, 1);
		uint16_t v = SFR_DATA_8 << 8 | SFR_DATA_0 | 0x4;
		print_string("\r\nv is now "); print_short(v);
		sds_write_v(0, 6, 1, v);
		delay(10);
		sds_read(0, 6, 1);
		v = SFR_DATA_8 << 8 | SFR_DATA_0 & 0xfb;
		sds_write_v(0, 6, 1, v);
	}
}


void phy_config(uint8_t phy)
{
	uint16_t pval;
	print_string("\r\nphy_config: ");
	write_char('0' + phy);

	sleep(20);
	// PHY configuration: External 8221B?
//	p081e.75f3:ffff P000100.1e0075f3:fffe
	phy_read(phy, 0x1e, 0x75f3);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 & 0xfe;
	phy_write(bit_mask[phy], 0x1e, 0x75f3, pval);
	sleep(20);

//	p081e.697a:ffff P000100.1e00697a:ffc1 / p031e.697a:0003 P000008.1e00697a:0001
	// SERDES OPTION 1 Register (MMD 30.0x6) bits 0-5: 0x01: Set HiSGMII+SGMII
	phy_read(phy, 0x1e, 0x697a);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 & 0xc0 | 0x01;
	phy_write(bit_mask[phy], 0x1e, 0x697a, pval);
	sleep(20);

//	p031f.a432:0811 P000008.1f00a432:0831
	// PHYCR2 PHY Specific Control Register 2, MMD 31. 0xA432), set bit 5: enable EEE
	phy_read(phy, 0x1f, 0xa432);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 | 0x20;
	phy_write(bit_mask[phy], 0x1f, 0xa432, pval);

//	p0307.003e:0000 P000008.0700003e:0001
	// EEE avertisment 2 register MMMD 7.0x003e, set bit 0: 2.5G has EEE capability
	phy_read(phy, 0x7, 0x3e);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 | 0x1;
	phy_write(bit_mask[phy], 0x7, 0x3e, pval);
	sleep(20);

//	p031f.a442:043c P000008.1f00a442:0430
	// Unknown, but clear bits 2/3
	phy_read(phy, 0x1f, 0xa442);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 & 0xf3;
	phy_write(bit_mask[phy], 0x1f, 0xa442, pval);
	sleep(20);

	// P000100.1e0075b5:e084
	phy_write(bit_mask[phy], 0x1e, 0x75b5, 0xe084);
	sleep(20);

//	p031e.75b2:0000 P000008.1e0075b2:0060
	// set bits 5/6
	phy_read(phy, 0x1e, 0x75b2);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 | 0x60;
	phy_write(bit_mask[phy], 0x1e, 0x75b2, pval);
	sleep(20);

//	p081f.d040:ffff P000100.1f00d040:feff
	// LCR6 (LED Control Register 6, MMD 31.D040), set bits 8/9 to 0b10
	phy_read(phy, 0x1e, 0xd040);
	pval = (SFR_DATA_8 & 0xfc) | 0x02;
	pval <<= 8;
	pval |= SFR_DATA_0;
	phy_write(bit_mask[phy], 0x1e, 0xd040, pval);
	sleep(20);

//	p081f.a400:ffff P000100.1f00a400:ffff, then: p081f.a400:ffff P000100.1f00a400:bfff
//	p031f.a400:1040 P000008.1f00a400:5040, then: p031f.a400:5040 P000008.1f00a400:1040
	// FEDCR (Fast Ethernet Duplex Control Register, MMD 31.0xA400)
	// Set bit 14, sleep, then clear again, according to the datasheet these bits are reserved

	phy_read(phy, 0x1f, 0xa400);
	pval = SFR_DATA_8 | 0x40;
	pval <<= 8;
	pval |= SFR_DATA_0;
	phy_write(bit_mask[phy], 0x1f, 0xa400, pval);
	sleep(20);

	phy_read(phy, 0x1f, 0xa400);
	pval = SFR_DATA_8 & 0xbf;
	pval <<= 8;
	pval |= SFR_DATA_0;
	phy_write(bit_mask[phy], 0x1f, 0xa400, pval);
	sleep(20);

	print_string("\r\n  phy config done\r\n");
}

void led_config_9xh(void)
{
	reg_bit_set(0x65d8, 0x1d);

	print_string("\r\nB Reg LED_MODE: ");
	print_reg(RTL837X_REG_LED_MODE);
	reg_read_m(0x6520);
	sfr_mask_data(1, 0x1f, 0x6);
	sfr_mask_data(0, 0xe0, 0xa0);
	reg_write_m(0x6520);
	print_string("\r\nA Reg LED_MODE: ");
	print_reg(RTL837X_REG_LED_MODE);

	print_string("\r\nB Reg 0x65f8: ");
	print_reg(0x65f8);
	reg_read_m(0x65f8);
	sfr_mask_data(0, 0, 0x3);
	reg_write_m(0x65f8);
	print_string("\r\nA Reg 0x65f8: ");
	print_reg(0x65f8);

	REG_SET(0x65fc, 0xffffffff);
	print_string("\r\nReg 0x65fc: ");
	print_reg(0x65fc);

	// Set bits 0-3 of 0x6600 to 0xf
	// r6600:00000000 R6600-0000000f
	reg_read_m(0x6600);
	sfr_mask_data(0, 0, 0x0f);
	reg_write_m(0x6600);
	print_string("\r\nA Reg 0x6600: ");
	print_reg(0x6600);

	// Set bit 0x1d of 0x65dc, clear bit 1b: r65dc:5fffff00 R65dc-7fffff00 r65dc:7fffff00 R65dc-77ffff00
	reg_bit_set(0x65dc, 0x1d);
	reg_bit_clear(0x65dc, 0x1b);
	print_string("\r\nA Reg 0x65dc: ");
	print_reg(0x65dc);


	reg_bit_set(0x7f8c, 0x1b);

	REG_SET(0x6548, 0x0041017f);

	// Configure LED_SET_0 ledid 2
	// 6544:01411000 R6544-01410044
	reg_read_m(0x6544);
	sfr_data[2] = 0x00;
	sfr_data[3] = 0x44;
	reg_write_m(0x6544);
	print_string("\r\nReg 0x6544: ");
	print_reg(0x6544);

	reg_read_m(0x6528);
	sfr_mask_data(0, 0x0f, 0x0f);
	reg_write_m(0x6528);
	print_string("\r\nReg 0x6528: ");
	print_reg(0x6528);
}


void led_config(void)
{
	// LED initialization
	// r6520:0021fdb0 R6520-0021e7b0 r6520:0021e7b0 R6520-0021e6b0
	reg_read_m(RTL837X_REG_LED_MODE);
	sfr_mask_data(2, 0xe0, 0x23); 	// Mask blink rate field (0xe0), set blink rate and LED to solid (set bit 1 = bit 17 overall)
	// Configure led-mode (serial?)
	sfr_data[2] = 0xe6;
	sfr_data[3] = 0xb0;
	reg_write_m(RTL837X_REG_LED_MODE);
	print_string("\r\nA Reg LED_MODE: ");
	print_reg(RTL837X_REG_LED_MODE);

	// Clear bits 0,1 of 0x65f8
//	r65f8:00000018 R65f8-00000018
	reg_read_m(0x65f8);
	sfr_mask_data(0, 0x03, 0);
	reg_write_m(0x65f8);
	print_string("\r\nA Reg 0x65f8: ");
	print_reg(0x65f8);

	// Set 0x65fc to 0xfffff000
	// R65fc-fffff000
	REG_SET(0x65fc, 0xfffff000);
	print_string("\r\nA Reg 0x65fc: ");
	print_reg(0x65fc);

	// Set bits 0-3 of 0x6600 to 0xf
	// r6600:00000000 R6600-0000000f
	reg_read_m(0x6600);
	sfr_mask_data(0, 0, 0x0f);
	reg_write_m(0x6600);
	print_string("\r\nA Reg 0x6600: ");
	print_reg(0x6600);

	// Set bit 0x1d of 0x65dc, clear bit 1b: r65dc:5fffff00 R65dc-7fffff00 r65dc:7fffff00 R65dc-77ffff00
	reg_bit_set(0x65dc, 0x1d);
	reg_bit_clear(0x65dc, 0x1b);
	print_string("\r\nA Reg 0x65dc: ");
	print_reg(0x65dc);

	// Set bits 1b/1d of 0x7f8c: r7f8c:30000000 R7f8c-30000000 r7f8c:30000000 R7f8c-38000000
	reg_bit_set(0x7f8c, 0x1d);
	reg_bit_set(0x7f8c, 0x1b);
	print_string("\r\nA Reg 0x7f8c: ");
	print_reg(0x7f8c);

	// LED setup
	// r6520:0021fdb0 R6520-0021e7b0 r6520:0021e7b0 R6520-0021e6b0 r65f8:00000018 R65f8-00000018 R65fc-fffff000 r6600:00000000 R6600-0000000f r65dc:5fffff00 R65dc-7fffff00 r65dc:7fffff00 R65dc-77ffff00
	// r7f8c:30000000 R7f8c-30000000 r7f8c:30000000 R7f8c-38000000 R6548-00410175 r6544:01411000 R6544-01410044 r6528:00000000 R6528-00000011 r6450:000020e6 R6450-000000e6 r644c:0a418820 R644c-0a400820

	// Configure LED_SET_0, ledid 0/1
	// R6548-00410175
	REG_SET(0x6548, 0x00410175);
	print_string("\r\nA Reg 0x6548: ");
	print_reg(0x6548);

	// Configure LED_SET_0 ledid 2
	// 6544:01411000 R6544-01410044
	reg_read_m(0x6544);
	sfr_data[2] = 0x00;
	sfr_data[3] = 0x44;
	reg_write_m(0x6544);
	print_string("\r\nReg 0x6544: ");
	print_reg(0x6544);

	// Further configure LED_SET_0
	// r6528:00000000 R6528-00000011
	reg_read_m(0x6528);
	sfr_data[3] = 0x11;
	reg_write_m(0x6528);
	print_string("\r\nReg 0x6528: ");
	print_reg(0x6528);
}


void rtl8372_init(void)
{
	// From run, set bits 0-1 to 1
	print_string("\r\nrtl8372_init called\r\n");
	print_string("\r\nB Reg 0x7f90: ");
	// This register also concerns the clock frequency
	print_reg(0x7f90);
/*	reg_read_m(0x7f90);
	sfr_mask_data(0, 0, 3);
	reg_write_m(0x7f90);
	print_string("\r\nA Reg 0x7f90: ");
	print_reg(0x7f90);
*/

	// r6330:00015555 R6330-00005555 r6330:00005555 R6330-00005555
	print_string("\r\nB Reg 0x6330: ");
	print_reg(0x6330);
	reg_read_m(0x6330);
//	sfr_mask_data(0, 0, 0xc0);	// Set Bits 6, 7
	sfr_mask_data(2, 3, 0);	 	// Delete bits 16, 17
	reg_write_m(0x6330);
	print_string("\r\nA Reg 0x6330: ");
	print_reg(0x6330);

	// r6334:00000000 R6334-000001f8  RTL8373: r6334:00000000 R6334-000000ff
	print_string("\r\nB Reg 0x6334: ");
	print_reg(0x6334);
	reg_read_m(0x6334);		// Also in sdsMode_set
	if (isRTL8373) {
		sfr_mask_data(0, 0, 0xff);
	} else {
		sfr_mask_data(1, 0, 0x01); 	// Set bits 3-8, On RTL8373+8224 set bits 0-7
		sfr_mask_data(0, 0, 0xf8);
	}
	reg_write_m(0x6334);
	print_string("\r\nA Reg 0x6334: ");
	print_reg(0x6334);

	// Enable MDC
	// r6454:00000000 R6454-00007000 RTL837X_REG_SMI_CTRL
	print_string("\r\nB Reg RTL837X_REG_SMI_CTRL: ");
	print_reg(RTL837X_REG_SMI_CTRL);
	reg_read_m(RTL837X_REG_SMI_CTRL);
	sfr_mask_data(1, 0, 0x70); 	// Set bits 0xc-0xe to enable MDC for SMI0-SMI2
	reg_write_m(RTL837X_REG_SMI_CTRL);
	sleep(10);

	print_string("SMI_CTRL: ");
	print_reg(RTL837X_REG_SMI_CTRL);

	// get_chip_version

	if (isRTL8373)
		led_config_9xh();
	else
		led_config();

	sds_init();

	// Part of the SDS configuration, see sdsMode_set, set bits 0xa-0xe to 0
	// r6450:000020e6 R6450-000000e6
	reg_read_m(0x6450);
	sfr_mask_data(1, 0x7c, 0);
	reg_write_m(0x6450);
	print_string("\r\nReg 0x6450: ");
	print_reg(0x6450);

	// SDS bits f-13 set to 0: r644c:0a418820 R644c-0a400820
	reg_read_m(0x644c);
	sfr_mask_data(2, 0x0f, 0);
	sfr_mask_data(1, 0x80, 0);
	reg_write_m(0x644c);
	print_string("\r\nReg 0x644c: ");
	print_reg(0x644c);

	// PHY configuration: External 8221B?
	phy_config(8);
	// PHY configuration: all internal PHYs?
	phy_config(3);

	// Set the MAC SerDes mode. Bits 0-4: SDS 0, Bits 5-9: SDS 1. Bits set to 1f
	// r7b20:00000bff R7b20-00000bff r7b20:00000bff R7b20-00000bff r7b20:00000bff R7b20-000003ff r7b20:000003ff R7b20-000003e2 r7b20:000003e2 R7b20-000003e2
	reg_read_m(RTL837X_REG_SDS_MODES);
	sfr_mask_data(1, 0, 0x03);
	sfr_mask_data(0, 0, 0xe2);
	reg_write_m(RTL837X_REG_SDS_MODES);

	// r0b7c:000000d8 R0b7c-000000f8 r6040:00000030 R6040-00000031

	// r0a90:000000f3 R0a90-000000fc
	reg_read_m(0xa90);
	sfr_mask_data(0, 0x0f,0x0c);
	reg_write_m(0xa90);
	print_string("\r\nReg 0xa90: ");
	print_reg(0x644c);

	// Disable PHYs for configuration
	phy_write(0xf0,0x1f,0xa610,0x2858);

	// Set bits 0x13 and 0x14 of 0x5fd4
	// r5fd4:0002914a R5fd4-001a914a
	reg_bit_set(0x5fd4, 0x13);
	reg_bit_set(0x5fd4, 0x14);
	print_string("\r\nReg 0x5fd4: ");
	print_reg(0x644c);

	// Configure ports 3-8:
	/*
	* r1538:00000e33 R1538-00000e37 r1538:00000e37 R1538-00000e37 r1538:00000e37 R1538-00000f37
	* r1638:00000e33 R1638-00000e37 r1638:00000e37 R1638-00000e37 r1638:00000e37 R1638-00000f37
	* r1738:00000e33 R1738-00000e37 r1738:00000e37 R1738-00000e37 r1738:00000e37 R1738-00000f37
	* r1838:00000e33 R1838-00000e37 r1838:00000e37 R1838-00000e37 r1838:00000e37 R1838-00000f37
	* r1938:00000e33 R1938-00000e37 r1938:00000e37 R1938-00000e37 r1938:00000e37 R1938-00000f37
	* r1a38:00000e33 R1a38-00000e37 r1a38:00000e37 R1a38-00000e37 r1a38:00000e37 R1a38-00000f37
	*
	* RTL8373:
	* r1238:00000e33 R1238-00000e37 r1238:00000e37 R1238-00000e37 r1238:00000e37 R1238-00000f37 (identical)
	*/
	uint16_t reg = 0x1238; // Port base register for the bits we set
	uint8_t numPorts = 8;
	if (!isRTL8373) {
		numPorts = 6;
		reg += 0x300;
	}

	for (char i = 0; i < numPorts; i++) {
		print_string("\r\nRegs: ");
		print_reg(reg);
		reg_bit_set(reg, 0x2);
		reg_bit_set(reg, 0x4);
		reg_bit_set(reg, 0x8);
		print_string("now: ");
		print_reg(reg);
		reg += 0x100;
	}
	print_string("\r\n");

	// r0b7c:000000d8 R0b7c-000000f8 r6040:00000030 R6040-00000031
	reg_bit_set(0xb7c, 5);
	print_string("\r\nReg 0x0b7c: ");
	print_reg(0x0b7c);

	print_string("\r\nB Reg 0x6040: ");
	print_reg(RTL837X_REG_HW_CONF);
	reg_bit_set(RTL837X_REG_HW_CONF, 0);
	print_string("\r\nA Reg 0x6040: ");
	print_reg(RTL837X_REG_HW_CONF);

	// TODO: patch the PHYs

	// Re-enable PHY after configuration
	phy_write(0xf0,0x1f,0xa610,0x2058);;

	// Set bits 0xc-0x14 of 0x632c to 0x1f8, see rtl8372_init
	// r632c:00000540 R632c-001f8540
	reg_read_m(0x632c);
	sfr_mask_data(1, 0x70, 0x80);
	sfr_mask_data(2, 0x10, 0x1f);
	reg_write_m(0x632c);
	print_string("\r\nReg 0x632c: ");
	print_reg(0x632c);

	print_string("\r\nrtl8372_init done\r\n");
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


__code struct command commands[N_COMMANDS] = {
	{ "reset", 1 },
};


uint8_t cmd_compare(uint8_t start, uint8_t * __code cmd)
{
	signed char i;
	signed char j = 0;

	for (i = cmd_words_b[start]; i < cmd_words_b[start + 1] && sbuf[i] != ' '; i++) {
//		print_short(i); write_char(':'); print_short(j); write_char('#'); print_string("\r\n");
//		write_char('>'); write_char(cmd[j]); write_char('-'); write_char(sbuf[i]); print_string("\r\n");
		if (!cmd[j])
			return 1;
		if (sbuf[i] != cmd[j++])
			break;
	}
//	write_char('.'); print_short(i); write_char(':'); print_short(i);
	if (i >= cmd_words_b[start + 1] || sbuf[i] == ' ')
		return 1;
	return 0;
}


void setup_i2c(void)
{
	REG_SET(0x0414, 0);
	REG_SET(0x0418, 0x00100280);
	REG_SET(0x041c, 0);

	// HW Control register, enable I2C?
	reg_read_m(0x7f90);
	sfr_mask_data(3, 0x20, 0x00); // Clear bit 29
	sfr_mask_data(0, 0x60, 0x40); // Set bits 5-6 to 0b10
	reg_write_m(0x7f90);
	print_string("\r\nReg 0x7f90: ");
	print_reg(0x7f909);
}


void bootloader(void)
{
	ticks = 0;
	sbuf_ptr = 0;

	CKCON = 0;	// Initial Clock configuration
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

	// Set default for SFP pins so we can start up a module already inserted
	sfp_pins_last = 0x3; // signal LOS and no module inserted

	isRTL8373 = 1; // FIXME: See below
	reg_read(0x4);

// 	port_leds_on();
	print_string("\r\nStarting up...\r\n");
	print_string("  Flash controller\r\n");
	flash_init(0);
	print_string("  > OK\r\n current status: ");
	print_short(flash_read_status());

	// The following will only show something else then 0xff if it was programmed for a managed switch
	print_string("\r\n  Testing read Securty Register 1\r\n");
	flash_read_security(0x0001000, 40);
	print_string("\r\n  Testing read Securty Register 2\r\n");
	flash_read_security(0x0002000, 40);
	print_string("\r\n  Testing read Securty Register 3\r\n");
	flash_read_security(0x0003000, 40);

	print_string("  > Flash status: ");
	print_short(flash_read_status());
	print_string("\r\n  Dumping flash at 0x100\r\n");
	flash_dump(0x100, 252);
	rtl8372_init();

	nic_setup();

	setup_i2c();

	print_string(greeting);
	print_string("\r\nCPU version: ");
	print_reg(0x4);
	print_string("\r\nClock register: ");
	print_reg(0x6040);

	print_string("\r\n> ");
	char l = sbuf_ptr;
	char line_ptr = l;
	char is_white = 1;
	while (1) {
		while (l != sbuf_ptr) {
			write_char(sbuf[l]);
			// Check whether there is a full line:
			if (sbuf[l] == '\n' || sbuf[l] == '\r') {
				print_long(ticks);
				// Print line and parse command into words
				print_string("\r\n  CMD: ");
				is_white = 1;
				uint8_t word = 0;
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
				if (i >= 0 && cmd_words_b[1] >= 0) {
					// print_string("\r\n  THERE may be a command: ");
					// print_short(i); print_string("\r\n");
					// print_short(cmd_words_b[0]); print_string("\r\n");
					// print_short(cmd_words_b[1]); print_string("\r\n");
					// print_short(cmd_words_b[2]); print_string("\r\n");
					// print_short(cmd_words_b[3]); print_string("\r\n");
					if (cmd_compare(0, "reset")) {
						print_string("\r\nRESET\n\n");
						reset_chip();
					}
					if (cmd_compare(0, "sfp")) {
						uint8_t rate = sfp_read_reg(12);
						print_string("\r\nRate: "); print_byte(rate);
						print_string("  Encoding: "); print_byte(sfp_read_reg(11));
						print_string("\r\n");
						for (uint8_t i = 20; i < 60; i++) {
							uint8_t c = sfp_read_reg(i);
							if (c)
								write_char(c);
						}
					}
					if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'd') {
						print_string("\r\nDUMPING FLASH\r\n");
						flash_dump(0, 255);
					}
					if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'j') {
						print_string("\r\nJEDEC ID\r\n");
						flash_read_jedecid();
					}
					if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'u') {
						print_string("\r\nUNIQUE ID\r\n");
						flash_read_uid();
					}
					// Switch to flash 62.5 MHz mode
					if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 's') {
						print_string("\r\nFLASH FAST MODE\r\n");
						flash_init(1);
						print_string("\r\nNow dumping flash\r\n");
						flash_dump(0, 255);
					}
					if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'e') {
						print_string("\r\nFLASH erase\r\n");
						flash_block_erase(0x20000);
					}
					if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'w') {
						print_string("\r\nFLASH write\r\n");
						for (uint8_t i = 0; i < 20; i++)
							flash_buf[i] = greeting[i];
						flash_write_bytes(0x20000, flash_buf, 20);
					}
				}
				print_string("\r\n> ");
			}
			l++;
			l &= (SBUF_SIZE - 1);
		}
		idle(); // Enter Idle mode until interrupt occurs
	}
}

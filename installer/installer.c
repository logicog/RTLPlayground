#include <8051.h>
#include <stdint.h>

// #define REGDBG 1
// #define RXTXDBG 1

#define UPDATE_LOC	0x0001D000
#define HEADER_LENGTH	0x14
#define UPDATE_CODE_LOC	(UPDATE_LOC + HEADER_LENGTH)

#include "../rtl837x_sfr.h"
#include "../rtl837x_regs.h"

#define SYS_TICK_HZ 100
#define SERIAL_BAUD_RATE 57600
#define CLOCK_HZ 125000000

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

volatile __xdata uint32_t ticks;
volatile __xdata uint8_t sec_counter;
volatile __xdata uint16_t sleep_ticks;

// We buffer 1 sector as this is also the erase size
__xdata uint8_t buffer[0x1000];
__xdata uint8_t dio_enabled;

__code uint8_t * __code hex = "0123456789abcdef";

void isr_timer0(void) __interrupt(1)
{
	TR0 = 0;		// Stop timer 0
	TH0 = (0x10000 - (CLOCK_HZ / SYS_TICK_HZ / 32)) >> 8;
	TL0 = (0x10000 - (CLOCK_HZ / SYS_TICK_HZ / 32)) % 0xff;

	ticks++;
	if (sleep_ticks > 0)
		sleep_ticks--;
	sec_counter++;

	TR0 = 1;		// Re-start timer 0
}


void isr_ext0(void) __interrupt(0)
{
	EX0 = 0;	// Disable interrupt for the moment
	IT0 = 1;	// Trigger on falling edge of external interrupt
	EX0 = 1;	// Re-enable interrupt
}


void isr_ext1(void) __interrupt(2)
{
	EX1 = 0;
	EX1 = 1;
}


void write_char(char c)
{
	do {
	} while (TI == 0);
	TI = 0;
	if (c =='\n') {
		SBUF = '\r';
		do {
		} while (TI == 0);
		TI = 0;
	}
	SBUF = c;
}


void print_string(__code char *p)
{
	while (*p)
		write_char(*p++);
}


void print_byte(uint8_t a)
{
	write_char(hex[(a >> 4) & 0xf]);
	write_char(hex[a & 0xf]);
}

void print_short(uint16_t a)
{
	print_string("0x");
	for (signed char i = 12; i >= 0; i -= 4) {
		write_char(hex[(a >> i) & 0xf]);
	}
}

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

/*
 * Configure Memory Managed IO
 */
void flash_configure_mmio(void)
{
	// Set configuration for MMIO access by controller
	if (dio_enabled) {
		SFR_FLASH_MODEB = 0x18;
		SFR_FLASH_CMD_R = 0xbb;	// By default we read with Dual speed
		SFR_FLASH_DUMMYCYCLES = 4;
		return;
	}

	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0xb;	// By default we read with single speed
	SFR_FLASH_DUMMYCYCLES = 8;
}


/*
 * Initializes the flash controller for programmed control
 * The configuration options are not really understood, the SPI speed
 * seems to be directly linked to the CPU frequency
 * This configures fast single IO at 20.8 MHz when the CPU clock is at 20.8MHz
 * and 62.5MHz when the CPU clock is configured at 125MHz
 */
void flash_init(uint8_t enable_dio)
{
	if (enable_dio) {
		// Configure fast DIO via divider/DIO/SIOconfig = 4 and read-cmd being 0xbb (for mmio)
		SFR_FLASH_CONFIG = 9;  // There may be a chip-select in here
		SFR_FLASH_CONF_RCMD = 0xbb;
		SFR_FLASH_CONF_DIV = 4;
	} else {
		// Configure fast read via divider = 8 and read-cmd being 0xb (for mmio)
		SFR_FLASH_CONFIG = 9;
		SFR_FLASH_CONF_RCMD = 0xb;
		SFR_FLASH_CONF_DIV = 8;
	}
	// Test Controller Busy
	while(SFR_FLASH_EXEC_BUSY);

	// Write 0 to status register
	SFR_FLASH_DUMMYCYCLES = 8;
	SFR_FLASH_MODEB = 0;
	SFR_FLASH_TCONF = 0x19;
	SFR_FLASH_CMD = 1;
	SFR_FLASH_DATA0 = 0;
	SFR_FLASH_EXEC_GO = 1;
	while(SFR_FLASH_EXEC_BUSY);

	dio_enabled = enable_dio;
	flash_configure_mmio();
}


uint8_t flash_read_status(void)
{
	// Test Controller Busy (we might call this directly after executing a command)
	while(SFR_FLASH_EXEC_BUSY);

	// setup status read command
	SFR_FLASH_TCONF = 0x11;
	SFR_FLASH_CMD_R = 5;

	// execute and wait for controller done
	SFR_FLASH_EXEC_GO = 1;
	while(SFR_FLASH_EXEC_BUSY);

	return SFR_FLASH_DATA0;
}


/*
 * Reads bulk data of length len from the flash memory starging at address src
 * and writes the data into a buffer pointed to by dst in XMEM
 */
void flash_read_bulk(register __xdata uint8_t *dst, __xdata uint32_t src, register uint16_t len)
{
	short status;
	do {
		status = flash_read_status();
	} while (status & 0x1);

	// Set fast read mode
	if (dio_enabled) {
		SFR_FLASH_MODEB = 0x18;
		SFR_FLASH_CMD_R = 0xbb;
		SFR_FLASH_DUMMYCYCLES = 4;
	} else {
		SFR_FLASH_MODEB = 0x0;
		SFR_FLASH_CMD_R = 0xb;	// Fast read
		SFR_FLASH_DUMMYCYCLES = 8;	// Add 8 dummy clocks after read?
	}
	// Read 4 bytes
	SFR_FLASH_TCONF = 4;
	while (len) {
		SFR_FLASH_ADDR16 = src >> 16;
		SFR_FLASH_ADDR8 = src >> 8;
		SFR_FLASH_ADDR0 = src;
		src += 4;

		SFR_FLASH_EXEC_GO = 1;
		while(SFR_FLASH_EXEC_BUSY);

		*dst++ = SFR_FLASH_DATA0;
		if (len == 1)
			return;
		*dst++ = SFR_FLASH_DATA8;
		if (len == 2)
			return;
		*dst++ = SFR_FLASH_DATA16;
		if (len == 3)
			return;
		*dst++ = SFR_FLASH_DATA24;

		len -= 4;
	}
}


void flash_write_enable(void)
{
	short status;

	// Wait until busy bit clear
	do {
		status = flash_read_status();
	} while (status & 0x1);
// 	while (flash_read_status() & 0x1);

	SFR_FLASH_TCONF = 0x18;
	SFR_FLASH_CMD = 6;
	SFR_FLASH_DUMMYCYCLES = 0;
	SFR_FLASH_MODEB = 0;

	SFR_FLASH_EXEC_GO = 1;
	// Wait for write status enabled
	do {
		status = flash_read_status();
	} while (!(status & 0x2));
}


// Erases the 4k sector in which the address lies
void flash_sector_erase(uint32_t addr)
{
	flash_write_enable();
	SFR_FLASH_TCONF = 8;
	SFR_FLASH_CMD = 0x20;

	SFR_FLASH_ADDR16 = addr >> 16;
	SFR_FLASH_ADDR8 = addr >> 8;
	SFR_FLASH_ADDR0 = addr;

	SFR_FLASH_EXEC_GO = 1;
	while (flash_read_status() & 0x1);

	flash_configure_mmio();

}


void flash_write_bytes(__xdata uint32_t addr, __xdata uint8_t *ptr, uint16_t len)
{
	uint8_t exit_loop = 0;

	while(1) {
		flash_write_enable();
		SFR_FLASH_CMD = 2;
		SFR_FLASH_TCONF = 0x40 | 8 | 4; // Bytes written is 4, 8 enables write, 0x40 is unknown
		// Last transfer?
		if (len < 5) {
			SFR_FLASH_TCONF = 8 | len;
			exit_loop = 1;
		}

		SFR_FLASH_ADDR16 = addr >> 16;
		SFR_FLASH_ADDR8 = addr >> 8;
		SFR_FLASH_ADDR0 = addr;
		SFR_FLASH_DATA0 = *ptr++;
		SFR_FLASH_DATA8 = *ptr++;
		SFR_FLASH_DATA16 = *ptr++;
		SFR_FLASH_DATA24 = *ptr++;

		// Execute transfer, we wait for completion at top of loop
		SFR_FLASH_EXEC_GO = 1;
		if (exit_loop)
			break;
		len -= 4;
		addr += 4;
	}

	while (flash_read_status() & 0x1);
	flash_configure_mmio();
}


void reg_write(uint16_t reg_addr)
{
	/* Data to write must be in SFR A4, A5, A6, A7 */
	SFR_REG_ADDR_U16 = reg_addr;
	SFR_EXEC_GO = SFR_EXEC_WRITE_REG;
	do {
	} while (SFR_EXEC_STATUS != 0);
}


void installer(void)
{

	CKCON = 0;	// Initial Clock configuration
	SFR_97 = 0;	// HADDR?

	// Set in managed mode:
	SFR_b9 = 0x00;
	SFR_ba = 0x80;

	// Disable all interrupts (global and individually) by setting IE register (SFR A8) to 0
	IE = 0;
	EIE = 0;  // SFR e8: EIE. Disable all external IRQs

	// Disable all interrupts (global interrupt enable bit)
	EA = 0; // SFR A8.7 / IE.7

	setup_serial();
	print_string("\nRTLPlayground installer starting...\n");

	// Initialize flash functions with disable DIO because writing does not work otherwise
	flash_init(0);

	__xdata uint32_t dest = 0x0;
	__xdata uint32_t source = UPDATE_CODE_LOC;
	// A 512kByte = 4MBit Flash has 128 sectors, we copy only 120
	for (uint8_t i=0; i < 120; i++) {
		print_string("Moving block\n");
		flash_read_bulk(buffer, source, 0x1000);
		for (uint8_t j = 0; j < 32; j++) {
			write_char(' '); print_byte(buffer[j]);
		}
		write_char('\n');
		flash_sector_erase(dest);
		flash_write_bytes(dest, buffer, 0x1000);
		dest += 0x1000;
		source += 0x1000;
	}
	print_string("Done.\n");
	print_string("Reseting now\n");
	REG_SET(RTL837X_REG_RESET, 1);
}

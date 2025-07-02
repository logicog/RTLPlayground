#include <8051.h>
#include <stdint.h>

// #define REGDBG 1
// #define RXTXDBG 1

#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_common.h"
#include "rtl837x_flash.h"
#include "rtl837x_phy.h"
#include "rtl837x_port.h"

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

__code uint8_t ownIP[] = { 192, 168, 2, 2 };
__code uint8_t ownMAC[] = { 0x1c, 0x2a, 0xa3, 0x23, 0x00, 0x02 };
__code uint8_t gatewayIP[] = { 192, 168, 2, 1};

__xdata uint8_t isRTL8373;

volatile __xdata uint32_t ticks;
volatile __xdata uint8_t sec_counter;
volatile __xdata uint16_t sleep_ticks;

#define N_WORDS 10
__xdata signed char cmd_words_b[N_WORDS];

// Buffer for serial input, SBUF_SIZE must be power of 2 < 256
#define SBUF_SIZE 32
__xdata char sbuf_ptr;
__xdata uint8_t sbuf[SBUF_SIZE];
__xdata uint8_t sfr_data[4];

__code uint8_t * __code greeting = "\r\nA minimal prompt to explore the RTL8372:\r\n";
__code uint8_t * __code hex = "0123456789abcdef";

__xdata uint8_t flash_buf[256];

// For RX data, a propriatary RTL FRAME is inserted. Instead of 0x0800 for IPv4,
// the RTL_FRAME_TAG_ID is used as part of an 8-byte tag
#define RTL_TAG_SIZE		8
#define RTL_FRAME_TAG_ID	0x8899

// For RX and TX, an 8 byte header describing the frame to be moved to the Asic
// and received from the Asic is used
#define RTL_FRAME_HEADER_SIZE	8

// This is the standard size of n Ethernet frame header
#define ETHER_HEADER_SIZE	14

__xdata uint8_t rx_headers[32];
__xdata uint8_t rx_buf[2048];	// FIXME: Currently no maximum packet size checked
__xdata uint8_t tx_buf[2048];
__xdata uint8_t tx_seq;
__xdata uint32_t ipv4_checksum;	// Note that this is little endian

__xdata uint8_t minPort;
__xdata uint8_t maxPort;
__xdata uint8_t nSFPPorts;


__xdata uint8_t was_offline;
__code uint8_t arp_broadcast[] = {
	0x00, 0x07, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,				// HEADER, BYTES 4/5: LEN FIXME
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1c, 0x2a, 0xa3, 0x23, 0x00, 0x01,	// BROADCAST-MAC, OWN MAC
	0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01,
	0x1c, 0x2a, 0xa3, 0x23, 0x00, 0x01,					// MAC ADRESS
	0xc0, 0xa8, 0x02, 0x02,							// IP ADDRESS
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,					// Target MAC
	0xc0, 0xa8, 0x02, 0x01,							// Target IP
	// Padding
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};


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
	if (sleep_ticks > 0)
		sleep_ticks--;
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
}


/*
 * External IRQ 0 Service Routine: Called on link change?
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
#ifdef REGDBG
	if (EA) { write_char('r'); print_byte(reg_addr >> 8); print_byte(reg_addr); write_char(':'); }
#endif
	SFR_REG_ADDRH = reg_addr >> 8;
	SFR_REG_ADDRL = reg_addr;
	SFR_EXEC_GO = SFR_EXEC_READ_REG;
	do {
	} while (SFR_EXEC_STATUS != 0);
	sfr_data[0] = SFR_DATA_24;
	sfr_data[1] = SFR_DATA_16;
	sfr_data[2] = SFR_DATA_8;
	sfr_data[3] = SFR_DATA_0;
#ifdef REGDBG
	if (EA) { print_byte(sfr_data[0]);  print_byte(sfr_data[1]);  print_byte(sfr_data[2]);  print_byte(sfr_data[3]); write_char(' '); }
#endif
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
#ifdef REGDBG
	if (EA) {
		write_char('R'); print_byte(reg_addr >> 8); print_byte(reg_addr); write_char('-');
		print_byte(sfr_data[0]);  print_byte(sfr_data[1]);  print_byte(sfr_data[2]);  print_byte(sfr_data[3]); write_char(' ');
	}
#endif
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


/*
 * Transfer Network Interface RX data from the ASIC to the 8051 XMEM
 * data will be stored in the rx_header structure
 * len is the length of data to be transferred
 */
void nic_rx_header(uint16_t ring_ptr)
{
	uint16_t buffer = (uint16_t) &rx_headers[0];
	SFR_NIC_DATA_H = buffer >> 8;
	SFR_NIC_DATA_L = buffer;
	SFR_NIC_RING_L = ring_ptr;
	SFR_NIC_RING_H = ring_ptr >> 8;
	SFR_NIC_CTRL = 1;
	do { } while (SFR_NIC_CTRL != 0);
}


/*
 * Transfer Network Interface RX data from the ASIC to the 8051 XMEM
 * the description of the packet must be in the rx_headers data structure
 * data will be returned in the xmem buffer points to
 * ring_ptr is the current position of the RX Ring on the ASIC side
 */
void nic_rx_packet(uint16_t buffer, uint16_t ring_ptr)
{
	SFR_NIC_DATA_H = buffer >> 8;
	SFR_NIC_DATA_L = buffer;
	SFR_NIC_RING_L = ring_ptr;
	SFR_NIC_RING_H = ring_ptr >> 8;
	uint16_t len = (((uint16_t)rx_headers[5]) << 8) | rx_headers[4];
	len += 7;
	len >>= 3;
#ifdef RXTXDBG
	print_string(" len: ");
	print_short(len);
#endif
	SFR_NIC_CTRL = len;
	do { } while (SFR_NIC_CTRL != 0);
}


void nic_tx_packet(uint16_t ring_ptr)
{
	uint16_t buffer = (uint16_t) tx_buf;
	SFR_NIC_DATA_H = buffer >> 8;
	SFR_NIC_DATA_L = buffer;
	ring_ptr <<= 3;
	ring_ptr |= 0x8000;
	SFR_NIC_RING_L = ring_ptr;
	SFR_NIC_RING_H = ring_ptr >> 8;
	uint16_t len =(((uint16_t)tx_buf[5]) << 8) | tx_buf[4];
	len += 0xf;
	len >>= 3;
	SFR_NIC_CTRL = len;
	do { } while (SFR_NIC_CTRL != 0);
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
	uint8_t current_bank = PSBANK;

	PSBANK = bank;
	v = *addr;
	PSBANK = current_bank;
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
#ifdef REGDBG
	print_string("q"); print_byte(sds_id); print_byte(page); print_byte(reg);
#endif
	SFR_93 = reg;			// 93
	SFR_94 = page << 1 | sds_id;	// 94
	SFR_EXEC_GO = SFR_EXEC_READ_SDS;
	do {
	} while (SFR_EXEC_STATUS != 0);

#ifdef REGDBG
	write_char(':'); print_byte(SFR_DATA_8); print_byte(SFR_DATA_0); write_char(' ');
#endif
}


/*
 * Write a SerDes register in the SoC
 * Input must be: sds_id = 0/1, page < 128,  reg <= 0xff
 * The value written must be in SFR A6 and A7 (SFR_DATA_8, SFR_DATA_0)
 */
void sds_write_v(uint8_t sds_id, uint8_t page, uint8_t reg, uint16_t v)
{
#ifdef REGDBG
	print_string("Q"); print_byte(sds_id); print_byte(page); print_byte(reg);
	write_char(':'); print_byte(v >> 8); print_byte(v); write_char(' ');
#endif
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
	write_char(hex[sfr_data[0] >> 4]);
	write_char(hex[sfr_data[0] & 0xf]);
	write_char(hex[sfr_data[1] >> 4]);
	write_char(hex[sfr_data[1] & 0xf]);
	write_char(hex[sfr_data[2] >> 4]);
	write_char(hex[sfr_data[2] & 0xf]);
	write_char(hex[sfr_data[3] >> 4]);
	write_char(hex[sfr_data[3] & 0xf]);
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
	reg_read_m(reg);
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


void sds_config_mac(uint8_t sds, uint8_t mode)
{
	reg_read_m(RTL837X_REG_SDS_MODES);
	sfr_data[0] = 0;
	sfr_data[1] = 0;
	if (!sds) {
		sfr_mask_data(0, 0x1f, mode);
	} else {
		sfr_mask_data(0, 0xe0, mode << 5);
		sfr_mask_data(1, 0xf3, mode >> 3);
	}
	if (isRTL8373) // Set 3rd SERDES Mode to 0x2:
		sfr_data[2] |= 0x08;
	reg_write_m(RTL837X_REG_SDS_MODES);
	print_string("\r\nRTL837X_REG_SDS_MODES: ");
	print_reg(RTL837X_REG_SDS_MODES);
	print_string("\r\n");
}

// Delay for given number of ticks without doing housekeeping
void delay(uint16_t t)
{
	sleep_ticks = t;
	while (sleep_ticks > 0)
		PCON |= 1;
}


void sds_config_8224(uint8_t sds)
{
	REG_SET(RTL837X_REG_SDS_MODES, 0xbed);
	delay(10);
	sds_config_mac(sds, 0x0d);

	/* Q002110:4444 Q002113:0404 Q002118:6d6d Q00211b:4242 Q00211d:0000 Q00361c:1313 Q003614:0000 Q003610:0202 Q002e04:0000 Q002e06:0404 Q002e07:0202 Q002e09:0606 Q002e0b:2222 Q002e0c:a2a2 Q002e0d:fefe Q002e15:f5f5
	 * Q002e16:0404 Q002e1d:abab Q000612:5050 Q000706:9494 Q000708:9494 Q00070a:9494 Q00070c:9494 Q001f0b:0000 Q000603:c4c4 q002000:0000 Q002000:0000 q002000:0030 Q002000:0000 q002000:0010 Q002000:0000 q002000:0050
	 * Q002000:0000 q002000:00d0 Q002000:0c0c q002000:0cd0 Q002000:0404 q002000:04d0 Q002000:0404 q002000:04d0 Q002000:0c0c q002000:0cd0 Q002000:0000 q002000:00d0 Q002000:0000 q002000:00d0 Q002000:0000 q002000:0050
	 * Q002000:0000 q002000:0010 Q002000:0000 q002000:0010 Q002000:0000 q002000:0030 Q002000:0000 q001f00:0000 Q001f00:0000 q001f00:000b Q001f00:0000
	*/
	// q000601:c800 Q000601:c8c8 q000601:c804 Q000601:c8c8 <<<<<<<<<<<<<<<<<<< CHECK THIS

	// Configure the SERDES LINK mode on the RTL8273 side, see also sds_config for RTL8221 and SFP ports
	// Q002110:4444 Q002113:0404 Q002118:6d6d Q00211b:4242 Q00211d:0000 Q00361c:1313 Q003614:0000 Q003610:0202 Q002e04:0000 Q002e06:0404 Q002e07:0202 Q002e09:0606 Q002e0b:2222 Q002e0c:a2a2 Q002e0d:fefe
	// Q002e15:f5f5 Q002e16:0404 Q002e1d:abab Q000612:5050 Q000706:9494 Q000708:9494 Q00070a:9494 Q00070c:9494 Q001f0b:0000 Q000603:c4c4
	sds_write_v(sds, 0x21, 0x10, 0x4444); delay(10); // Q002110:4444
	sds_write_v(sds, 0x21, 0x13, 0x0404); delay(10); // Q002113:0404
	sds_write_v(sds, 0x21, 0x18, 0x6d6d); delay(10); // Q002118:6d6d
	sds_write_v(sds, 0x21, 0x1b, 0x4242); delay(10); // Q00211b:4242
	sds_write_v(sds, 0x21, 0x1d, 0x0000); delay(10); // Q00211d:0000
	sds_write_v(sds, 0x36, 0x1c, 0x1313); delay(10); // Q00361c:1313
	sds_write_v(sds, 0x36, 0x14, 0x0000); delay(10); // Q003614:0000
	sds_write_v(sds, 0x36, 0x10, 0x0202); delay(10); // Q003610:0202
	sds_write_v(sds, 0x2e, 0x04, 0x0000); delay(10); // Q002e04:0000
	sds_write_v(sds, 0x2e, 0x06, 0x0404); delay(10); // Q002e06:0404
	sds_write_v(sds, 0x2e, 0x07, 0x0202); delay(10); // Q002e07:0202
	sds_write_v(sds, 0x2e, 0x09, 0x0606); delay(10); // Q002e09:0606
	sds_write_v(sds, 0x2e, 0x0b, 0x2222); delay(10); // Q002e0b:2222
	sds_write_v(sds, 0x2e, 0x0c, 0xa2a2); delay(10); // Q002e0c:a2a2
	sds_write_v(sds, 0x2e, 0x0d, 0xfefe); delay(10); // Q002e0d:fefe
	sds_write_v(sds, 0x2e, 0x15, 0xf5f5); delay(10); // Q002e15:f5f5
	sds_write_v(sds, 0x2e, 0x16, 0x0404); delay(10); // Q002e16:0404
	sds_write_v(sds, 0x2e, 0x1d, 0xabab); delay(10); // Q002e1d:abab
	sds_write_v(sds, 0x06, 0x12, 0x5050); delay(10); // Q000612:5050
	sds_write_v(sds, 0x07, 0x06, 0x9494); delay(10); // Q000706:9494
	sds_write_v(sds, 0x07, 0x08, 0x9494); delay(10); // Q000708:9494
	sds_write_v(sds, 0x07, 0x0a, 0x9494); delay(10); // Q00070a:9494
	sds_write_v(sds, 0x07, 0x0c, 0x9494); delay(10); // Q00070c:9494
	sds_write_v(sds, 0x1f, 0x0b, 0x0000); delay(10); // Q001f0b:0000
	sds_write_v(sds, 0x06, 0x03, 0xc4c4); delay(10); // Q000603:c4c4
	delay(500);
/*	// q002000:0000 Q002000:0000 q002000:0030 Q002000:0000 q002000:0010 Q002000:0000 q002000:0050 Q002000:0000 q002000:00d0
	sds_read(sds, 0x20, 0x00); v = ((uint16_t)SFR_DATA_8) << 8 | SFR_DATA_0; sds_write_v(sds, 0x20, 0x00, v);
	delay(10);
	sds_read(sds, 0x20, 0x00); v = ((uint16_t)SFR_DATA_8) << 8 | SFR_DATA_0; sds_write_v(sds, 0x20, 0x00, v | 0x30);
	delay(10);
	sds_read(sds, 0x20, 0x00); v = ((uint16_t)SFR_DATA_8) << 8 | SFR_DATA_0; sds_write_v(sds, 0x20, 0x00, v | 0x30);

	void sds_write(uint8_t sds_id, uint8_t page, uint8_t reg)
	sds_write_v(sds, 0x20, 0x00, 0x0c0c); // Q002000:0c0c
	sds_write_v(sds, 0x1f, 0x00, 0x0000); // Q001f00:0000 // q001f00:000b Q001f00:0000
	*/
}


void sds_config(uint8_t sds, uint8_t mode)
{
	sds_config_mac(sds, mode);

	if (mode == SDS_QXGMII) // A special mode for the RTL8224, SerDes configured as for 10GR
		mode = SDS_10GR;

	if (mode == SDS_10GR) // 10G Fiber
		sds_write_v(sds, 0x21, 0x10, 0x4480); // Q002110:6480
	else
		sds_write_v(sds, 0x21, 0x10, 0x6480); // Q002110:6480
	sds_write_v(sds, 0x21, 0x13, 0x0400); // Q002113:0400
	sds_write_v(sds, 0x21, 0x18, 0x6d02); // Q002118:6d02
	sds_write_v(sds, 0x21, 0x1b, 0x424e); // Q00211b:424e
	sds_write_v(sds, 0x21, 0x1d, 0x0002); // Q00211d:0002
	sds_write_v(sds, 0x36, 0x1c, 0x1390); // Q00361c:1390
	sds_write_v(sds, 0x36, 0x14, 0x003f); // Q003614:003f

	uint8_t page = 0;
	uint16_t v = 0;
	print_string("\r\nTrying to set SDS mode to 0x");
	print_byte(mode);
	print_string("\r\n");

	switch (mode) {
	case SDS_SGMII:
	case SDS_1000BX_FIBER:
		v = 0x0300;
		page = 0x24;
		break;
	case SDS_HISGMII:
	case SDS_HSG:
		v = 0x0200;
		page = 0x28;
		break;
	case SDS_10GR:
		v = 0x0200;
		page = 0x2e;
		break;
	default:
		print_string("Error in SDS Mode\r\n");
		return;
	}
	sds_write_v(sds, 0x36, 0x10, v); // Q003610:0200

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

	REG_WRITE(0x0420, 0, 0, 0, reg);

	// Execute I2C Read
	reg_bit_set(0x418, 0);

	// Wait for execution to finish
	do {
		reg_read_m(0x418);
	} while (sfr_data[3] & 0x1);

	reg_read_m(0x0424);
	return sfr_data[3];
}


void prepare_arp(uint8_t broadcast)
{
	for (uint8_t i = 0; i < arp_broadcast[4]; i++)
		tx_buf[i] = arp_broadcast[i];
	tx_buf[0] = tx_seq++;
	for (uint8_t i = 0; i < 6; i++)
		tx_buf[14 + i] = tx_buf[30 + i] = ownMAC[i];
	for (uint8_t i = 0; i < 4; i++)
		tx_buf[36 + i] = ownIP[i];
	if (broadcast) {
		for (uint8_t i = 0; i < 4; i++)
			tx_buf[46 + i] = ownIP[i];	// An annoucement, using own IP. Will also need to ask for GW
	} else {
		for (uint8_t i = 0; i < 4; i++)
			tx_buf[46 + i] = gatewayIP[i];
	}
}


void prepare_icmp_reply(void)
{
	for (uint8_t i = 0; i < arp_broadcast[4]; i++)
		tx_buf[i] = arp_broadcast[i];
	tx_buf[0] = tx_seq++;
	for (uint8_t i = 0; i < 6; i++)
		tx_buf[8 + i] = rx_buf[6 + i];
	for (uint8_t i = 0; i < 6; i++)
		tx_buf[14 + i] = ownMAC[i];
	tx_buf[20] = 0x08; tx_buf[21] = 0; tx_buf[22] = 0x45; tx_buf[0x23] = 0x00;
	tx_buf[28] = 0x40; tx_buf[29] = 0x00;	// DONT FRAG, FRAG 0
	tx_buf[26] = 0xef; tx_buf[27] = 0xdf;	// ID
	tx_buf[30] = 0x40; tx_buf[31] = 0x01;	// TTL, ICMP
	for (uint8_t i = 0; i < 4; i++)
		tx_buf[34 + i] = ownIP[i];
	// RTL Tag after dest-mac and source-mac: 8 Bytes
	for (uint8_t i = 0; i < 4; i++)
		tx_buf[26 + i] = rx_buf[26 + i];
	for (uint8_t i = 0; i < 4; i++)		// DEST-IP
		tx_buf[38 + i] = rx_buf[34 + i];
	tx_buf[4] = tx_buf[25] = 84;		// TCP length
	tx_buf[4] = 84 + ETHER_HEADER_SIZE;	// Total Ethernet frame len
	tx_buf[5] = tx_buf[24] = 0;
	for (uint8_t i = 0; i < 60; i++)	// Copy sequence number, id, timestamp and data over
		tx_buf[RTL_FRAME_HEADER_SIZE + 38 + i] = rx_buf[RTL_TAG_SIZE + 38 + i];
}


void handle_rx(void)
{
	reg_read_m(RTL837X_REG_RX_AVAIL);
	if (sfr_data[2] != 0 || sfr_data[3] != 0) {
#ifdef RXTXDBG
		print_string("\r\nrx:");
		print_long_x(sfr_data);
#endif
		reg_read_m(RTL837X_REG_RX_RINGPTR);
#ifdef RXTXDBG
		print_string(", ");
		print_long_x(sfr_data);
#endif
		uint16_t ring_ptr = ((uint16_t)sfr_data[2]) << 8;
		ring_ptr |= sfr_data[3];
		ring_ptr <<= 3;
#ifdef RXTXDBG
		print_string(", ring_ptr: ");
		print_short(ring_ptr);
#endif
		nic_rx_header(ring_ptr);
		__xdata uint8_t *ptr = rx_headers;
#ifdef RXTXDBG
		print_string(", on port "); print_byte(rx_headers[3] & 0xf);
		print_string(": ");
		for (uint8_t i = 0; i < 8; i++) {
			print_byte(*ptr++);
			write_char(' ');
		}
#endif

		nic_rx_packet((uint16_t) rx_buf, ring_ptr + 8);
#ifdef RXTXDBG
		print_string("\r\n<< ");
		ptr = rx_buf;
		for (uint8_t i = 0; i < 80; i++) {
			print_byte(*ptr++);
			write_char(' ');
		}
#endif

		sfr_data[0] = sfr_data[1] = sfr_data[2] = 0;
		sfr_data[3] = 0x1;
		reg_write_m(RTL837X_REG_RX_DONE);

		// Test, wether we have to react to the packet in any way
		if (was_offline) {	// Need to send an ARP broadcast for our GW?
			prepare_arp(1);
			was_offline = 0;
		} else if (rx_buf[0] == 0xff && rx_buf[1] == 0xff && rx_buf[2] == 0xff			// Broadcast?
				&& rx_buf[3] == 0xff && rx_buf[4] == 0xff && rx_buf[5] == 0xff) {
			prepare_arp(0);
#ifdef RXTXDBG
			print_string("\r\nBROADCAST\r\n");
#endif
		}  else if (rx_buf[0] == ownMAC[0] && rx_buf[1] == ownMAC[1] && rx_buf[2] == ownMAC[2]
				&& rx_buf[3] == ownMAC[3] && rx_buf[4] == ownMAC[4] && rx_buf[5] == ownMAC[5]) {
			if (rx_buf[31] == 0x01) {
#ifdef RXTXDBG
				print_string("ICMP PING REQ\r\n");
#endif
				prepare_icmp_reply();
			} else {
				return; // We only answer to ICMP PING requests
			}
		} else {
			return;
		}

#ifdef RXTXDBG
		reg_read_m(0x7880);
		print_string("\r\nDO TX. 0x7880: ");
		print_long_x(sfr_data);
#endif
		reg_read_m(0x7890);
		ptr = tx_buf;
#ifdef RXTXDBG
		print_string(", 0x7890: ");
		print_long_x(sfr_data);
		print_string("\r\n>> ");
		for (uint8_t i = 0; i < 120; i++) {
			print_byte(*ptr++);
			write_char(' ');
		}
		print_string("\r\n> ");
#endif
		ring_ptr = ((uint16_t)sfr_data[2]) << 8;
		ring_ptr |= sfr_data[3];
		nic_tx_packet(ring_ptr);

		reg_read_m(0x7884);
#ifdef RXTXDBG
		print_string("New Ring Pointer: ");
		print_long_x(sfr_data);
		print_string(" (should be previous ptr, now)");
#endif
		sfr_data[0] = sfr_data[1] = sfr_data[2] = 0;
		sfr_data[3] = 0x1;
		reg_write_m(0x7850);
	}
}

void handle_sfp(void)
{
	reg_read_m(RTL837X_REG_GPIO_B);
	if ((sfp_pins_last & 0x1) && (!(sfr_data[0] & 0x40))) {
		sfp_pins_last &= ~0x01;
		print_string("\r\n<MODULE INSERTED>  ");
		// Read Reg 11: Encoding, see SFF-8472 and SFF-8024
		// Read Reg 12: Signalling rate (including overhead) in 100Mbit: 0xd: 1Gbit, 0x67:10Gbit
		delay(100); // Delay, because some modules need time to wake up
		uint8_t rate = sfp_read_reg(12);
		print_string("Rate: "); print_byte(rate);  // Normally 1, but 0 for DAC, can be ignored?
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

	reg_read_m(RTL837X_REG_GPIO_C);
	if ((sfp_pins_last & 0x2) && (!(sfr_data[3] & 0x20))) {
		sfp_pins_last &= ~0x02;
		print_string("\r\n<SFP-RX OK>\r\n");
	}
	if ((!(sfp_pins_last & 0x2)) && (sfr_data[3] & 0x20)) {
		sfp_pins_last |= 0x02;
		print_string("\r\n<SFP-RX LOS>\r\n");
	}
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
#ifdef DEBUG
		print_string("  sec_counter: "); print_byte(v);
#endif
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
		reg_read_m(RTL837X_REG_SEC_COUNTER);
#ifdef DEBUG
		print_string(" >>: ");
		print_long_x(sfr_data);
#endif
	}

	reg_read_m(RTL837X_REG_LINKS);
	if (!isRTL8373 && cmp_4(sfr_data, linkbits_last)) {
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

	// Check for changes with SFP modules
	handle_sfp();

	// Check new Packets RX
	handle_rx();
}


// Sleep the given number of ticks
void sleep(uint16_t t)
{
	sleep_ticks = t;
	while (sleep_ticks > 0)
		idle();
}

void reset_chip(void)
{
	REG_SET(RTL837X_REG_RESET, 1);
}


void setup_external_irqs(void)
{
	REG_SET(0x5f84, 0x42);
	REG_SET(0x5f34, 0x3ff);

	EX0 = 1;	// Enable external IRQ 0
	IT0 = 1;	// External IRQ on falling edge

	EX1 = 1;	// External IRQ 1 enable
	EX2 = 1;	// External IRQ 2 enable: bit EIE.0
	EX3 = 1;	// External IRQ 3 enable: bit EIE.1
	PX3 = 1;	// Set EIP.1 = 1: External IRQ 3 set to high priority
}


void rtl8224_enable(void)
{
	// Set Pin 4 low
	reg_bit_clear(RTL837X_REG_GPIO_A, 4);
	// Configure Pin as output
	reg_bit_set(RTL837X_REG_GPIO_CONF_A, 4);
	delay(10);
	// Set pin 4 high
	reg_bit_set(RTL837X_REG_GPIO_A, 4);
	delay(50);
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
 * Data to be written is in v
 */
void phy_write(uint16_t phy_mask, uint8_t dev_id, uint16_t reg, uint16_t v)
{
#ifdef REGDBG
	print_string("P"); print_byte(phy_mask>>8); print_byte(phy_mask); print_byte(dev_id); write_char('.'); print_byte(reg>>8); print_byte(reg); write_char(':');
	print_byte(v>>8); print_byte(v); write_char(' ');
#endif
	SFR_DATA_8 = v >> 8;			// SFR_A6
	SFR_DATA_0 = v;				// SFR_A7
	SFR_SMI_PHYMASK = phy_mask;		// SFR_C5
	SFR_SMI_REG_H = reg >> 8;		// SFR_C2
	SFR_SMI_REG_L = reg;			// SFR_C3
	SFR_SMI_DEV = (phy_mask >> 8) | dev_id  << 3 | 2; // SFR_C4: bit 2 can also be set for some option
	SFR_EXEC_GO = SFR_EXEC_WRITE_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);
}


/*
 * Read a phy register via MDIO clause 45
 * Input must be: phy_id < 64,  device_id < 32,  reg < 0x10000)
 * The result is in SFR A6 and A7 (SFR_DATA_8, SFR_DATA_0)
 */
void phy_read(uint8_t phy_id, uint8_t dev_id, uint16_t reg)
{
#ifdef REGDBG
	print_string("p"); print_byte(phy_id); print_byte(dev_id); write_char('.'); print_byte(reg>>8); print_byte(reg); write_char(':');
#endif
	SFR_SMI_REG_H = reg >> 8;	// c3
	SFR_SMI_REG_L = reg;		// c2
	SFR_SMI_PHY = phy_id;		// a5
	SFR_SMI_DEV = dev_id << 3 | 2;	// c4

	SFR_EXEC_GO = SFR_EXEC_READ_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);
#ifdef REGDBG
	print_byte(SFR_DATA_8); print_byte(SFR_DATA_0); write_char(' ');
#endif
}


void nic_setup(void)
{
	// Enable NIC
	// r6040:00000100 R6040-00001100
	reg_bit_set(RTL837X_REG_HW_CONF, 0xc);

	// This sets the size of the RX buffer, the filling level is in 0x7874
	// R7848-000004ff
	REG_SET(0x7848, 0x4ff);

	// R7844-000007fe
	REG_SET(0x7844, 0x7fe);

	// r785c:0401201e R785c-0401201e r785c:0401201e R785c-0400201e
	// 0x785c: Set bits 24-31 to 0x4, clear bits 16/17:
	reg_read_m(0x785c);
	sfr_mask_data(3, 0xff, 0x04);
	sfr_mask_data(2, 0x03, 0);
	reg_write_m(0x785c);

	// Set bit 0 of 0x7860:
	// r7860:00000000 R7860-00000001
	reg_bit_set(0x7860, 0);

	// r785c:0400201e R785c-0400201f
	reg_bit_set(0x785c, 0);

	// r785c:0400201f R785c-0400201b
	reg_bit_clear(0x785c, 2);

	// R603c-00000200
	REG_SET(0x603c, 0x200);

	// r6720:00000500 R6720-00000501 R6720-00000501 r6720:00000501
	reg_read_m(0x6720);
	sfr_mask_data(0, 1, 1);
	sfr_mask_data(1, 3, 0);
	reg_write_m(0x6720);

	// r6368:00000194 R6368-00000197
	reg_read_m(0x6368);
	sfr_mask_data(0, 0, 3);
	reg_write_m(0x6368);

	// Sequence number of TX packets
	tx_seq = 0;
}


/*
 * Configure the PHY-Side of the SDS-SDS link between SoC and PHY
 */
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
	phy_read(0, 0x1e, 0xd);
	uint16_t pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0;

	// PHY Initialization:
	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);
	delay(10);

	pval &= 0xfff0;
	pval |= 0x0a;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);

	phy_write(0x1, 0x1e, 0xd, pval);

	phy_read(0, 0x1e, 0xd);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0;

	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);

	delay(10);

	pval &= 0xfff0;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);

	delay(10);
	phy_write(0x1, 0x1e, 0xd, pval);
	delay(10);

	if (isRTL8373) {
/*		reg_read_m(RTL837X_REG_SDS_MODES);
		sfr_mask_data(1, 0xfc, 0x04);
		sfr_mask_data(0, 0x1f, 0xd);
		reg_write_m(RTL837X_REG_SDS_MODES);*/
		// Disable all SERDES for configuration
		REG_SET(RTL837X_REG_SDS_MODES, 0x000037ff);
		sds_read(0, 0x06, 0x01); sds_write_v(0, 0x06, 0x01, 0xc8c8); delay(20);
		sds_read(0, 0x06, 0x01); sds_write_v(0, 0x06, 0x01, 0xc8c8); delay(20);
	}
}





void led_config_9xh(void)
{
	// r65d8:3ffbedff R65d8-3ffbedff
	reg_bit_set(0x65d8, 0x1d);

	//  r6520:0021fdb0 R6520-0021e7b0 r6520:0021e7b0 R6520-0021e6b0
	reg_read_m(0x6520);
	sfr_mask_data(1, 0x1f, 0x6);
	sfr_mask_data(0, 0xe0, 0xa0);
	reg_write_m(0x6520);

	//  r65f8:00000018 R65f8-0000001b
	reg_read_m(0x65f8);
	sfr_mask_data(0, 0, 0x3);
	reg_write_m(0x65f8);

	// R65fc-ffffffff
	REG_SET(0x65fc, 0xffffffff);

	// r6528:00000000 R6528-0000000f
	reg_read_m(0x6528);
	sfr_mask_data(0, 0x0f, 0x0f);
	reg_write_m(0x6528);

	// Set bits 0-3 of 0x6600 to 0xf
	// r6600:00000000 R6600-0000000f
	reg_read_m(0x6600);
	sfr_mask_data(0, 0, 0x0f);
	reg_write_m(0x6600);

	// Set bit 0x1d of 0x65dc, clear bit 1b: r65dc:5fffff00 R65dc-7fffff00 r65dc:7fffff00 R65dc-77ffff00
	reg_bit_set(0x65dc, 0x1d);
	reg_bit_clear(0x65dc, 0x1b);

	// r7f8c:30000000 R7f8c-30000000 r7f8c:30000000 R7f8c-38000000
	reg_bit_set(0x7f8c, 0x1b);

	// R6548-0041017f
	REG_SET(0x6548, 0x0041017f);

	// Configure LED_SET_0 ledid 2
	//  r6544:01411000 R6544-01410044
	reg_read_m(0x6544);
	sfr_data[2] = 0x00;
	sfr_data[3] = 0x44;
	reg_write_m(0x6544);
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

	// Clear bits 0,1 of 0x65f8
//	r65f8:00000018 R65f8-00000018
	reg_read_m(0x65f8);
	sfr_mask_data(0, 0x03, 0);
	reg_write_m(0x65f8);

	// Set 0x65fc to 0xfffff000
	// R65fc-fffff000
	REG_SET(0x65fc, 0xfffff000);

	// Set bits 0-3 of 0x6600 to 0xf
	// r6600:00000000 R6600-0000000f
	reg_read_m(0x6600);
	sfr_mask_data(0, 0, 0x0f);
	reg_write_m(0x6600);

	// Set bit 0x1d of 0x65dc, clear bit 1b: r65dc:5fffff00 R65dc-7fffff00 r65dc:7fffff00 R65dc-77ffff00
	reg_bit_set(0x65dc, 0x1d);
	reg_bit_clear(0x65dc, 0x1b);

	// Set bits 1b/1d of 0x7f8c: r7f8c:30000000 R7f8c-30000000 r7f8c:30000000 R7f8c-38000000
	reg_bit_set(0x7f8c, 0x1d);
	reg_bit_set(0x7f8c, 0x1b);

	// LED setup
	// r6520:0021fdb0 R6520-0021e7b0 r6520:0021e7b0 R6520-0021e6b0 r65f8:00000018 R65f8-00000018 R65fc-fffff000 r6600:00000000 R6600-0000000f r65dc:5fffff00 R65dc-7fffff00 r65dc:7fffff00 R65dc-77ffff00
	// r7f8c:30000000 R7f8c-30000000 r7f8c:30000000 R7f8c-38000000 R6548-00410175 r6544:01411000 R6544-01410044 r6528:00000000 R6528-00000011 r6450:000020e6 R6450-000000e6 r644c:0a418820 R644c-0a400820

	// Configure LED_SET_0, ledid 0/1
	// R6548-00410175
	REG_SET(0x6548, 0x00410175);

	// Configure LED_SET_0 ledid 2
	// 6544:01411000 R6544-01410044
	reg_read_m(0x6544);
	sfr_data[2] = 0x00;
	sfr_data[3] = 0x44;
	reg_write_m(0x6544);

	// Further configure LED_SET_0
	// r6528:00000000 R6528-00000011
	reg_read_m(0x6528);
	sfr_data[3] = 0x11;
	reg_write_m(0x6528);

	reg_read_m(0x6450);
	sfr_mask_data(1, 0x7c, 0);
	reg_write_m(0x6450);

	// SDS bits f-13 set to 0: r644c:0a418820 R644c-0a400820
	reg_read_m(0x644c);
	sfr_mask_data(2, 0x0f, 0);
	sfr_mask_data(1, 0x80, 0);
	reg_write_m(0x644c);
}


void rtl8372_init(void)
{
	// From run, set bits 0-1 to 1
	print_string("\r\nrtl8372_init called\r\n");
/*	reg_read_m(0x7f90);
	sfr_mask_data(0, 0, 3);
	reg_write_m(0x7f90);
	print_string("\r\nA Reg 0x7f90: ");
	print_reg(0x7f90);
*/

	// r6330:00015555 R6330-00005555 r6330:00005555 R6330-00005555
	reg_read_m(0x6330);
//	sfr_mask_data(0, 0, 0xc0);	// Set Bits 6, 7
	sfr_mask_data(2, 3, 0);	 	// Delete bits 16, 17
	reg_write_m(0x6330);

	// r6334:00000000 R6334-000001f8  RTL8373: r6334:00000000 R6334-000000ff
	reg_read_m(0x6334);		// Also in sdsMode_set
	if (isRTL8373) {
		sfr_mask_data(0, 0, 0xff);
	} else {
		sfr_mask_data(1, 0, 0x01); 	// Set bits 3-8, On RTL8373+8224 set bits 0-7
		sfr_mask_data(0, 0, 0xf8);
	}
	reg_write_m(0x6334);

	// Enable MDC
	// r6454:00000000 R6454-00007000 RTL837X_REG_SMI_CTRL
	reg_read_m(RTL837X_REG_SMI_CTRL);
	sfr_mask_data(1, 0, 0x70); 	// Set bits 0xc-0xe to enable MDC for SMI0-SMI2
	reg_write_m(RTL837X_REG_SMI_CTRL);
	delay(10);

	// get_chip_version
	if (isRTL8373)
		led_config_9xh();
	else
		led_config();

	sds_init();

	if (isRTL8373) {
		sds_config_8224(0);
		phy_config_8224();
		// q012100:4902 Q012100:4949 q013605:0000 Q013605:4040 Q011f02:0000 q011f15:0086
		sds_write_v(1, 0x21, 0x00, 0x4949);
		sds_write_v(1, 0x36, 0x05, 0x4040);
		sds_write_v(1, 0x1f, 0x02, 0x0000);
		sleep(10);
		sds_read(1, 0x1f, 0x15);
		sleep(10);
	} else {
		phy_config(8);	// PHY configuration: External 8221B?
		phy_config(3);	// PHY configuration: all internal PHYs?
		// Set the MAC SerDes Modes Bits 0-4: SDS 0 = 0x2 (0x2), Bits 5-9: SDS 1: 1f (off)
		// r7b20:00000bff R7b20-00000bff r7b20:00000bff R7b20-00000bff r7b20:00000bff R7b20-000003ff r7b20:000003ff R7b20-000003e2 r7b20:000003e2 R7b20-000003e2
		reg_read_m(RTL837X_REG_SDS_MODES);
		sfr_mask_data(1, 0, 0x03);
		sfr_mask_data(0, 0, 0xe2);
		reg_write_m(RTL837X_REG_SDS_MODES);
	}

	// r0a90:000000f3 R0a90-000000fc
	reg_read_m(0xa90);
	sfr_mask_data(0, 0x0f,0x0c);
	reg_write_m(0xa90);

	if (isRTL8373)
		rtl8224_phy_enable();

	// Disable PHYs for configuration
	if (isRTL8373)
		phy_write(0xff,0x1f,0xa610,0x2858);
	else
		phy_write(0xf0,0x1f,0xa610,0x2858);

	// Set bits 0x13 and 0x14 of 0x5fd4
	// r5fd4:0002914a R5fd4-001a914a
	reg_bit_set(0x5fd4, 0x13);
	reg_bit_set(0x5fd4, 0x14);

	// Configure ports 3-8:
	/*
	* r1538:00000e33 R1538-00000e37 r1538:00000e37 R1538-00000e37 r1538:00000e37 R1538-00000f37
	* [...]
	*
	* RTL8373:
	* r1238:00000e33 R1238-00000e37 r1238:00000e37 R1238-00000e37 r1238:00000e37 R1238-00000f37 (identical)
	*/
	uint16_t reg = 0x1238; // Port base register for the bits we set
	minPort = 0;
	maxPort = 8;
	nSFPPorts = 1; // FIXME: It could also be 2
	if (!isRTL8373) {
		minPort = 3;
		maxPort = 8;
	}

	for (char i = 0; i < 9; i++) {
		if (i >= minPort && i <= maxPort) {
			reg_bit_set(reg, 0x2);
			reg_bit_set(reg, 0x4);
			reg_bit_set(reg, 0x8);
		}
		reg += 0x100;
	}

	// r0b7c:000000d8 R0b7c-000000f8 r6040:00000030 R6040-00000031
	reg_bit_set(0xb7c, 5);

	if (isRTL8373) {
		// R7124-00001050 R7128-00001050 R712c-00001050 R7130-00001050 R7134-00001050 R7138-00001050 R713c-00001050 R7140-00001050 R7144-00001050 R7148-00001050
		REG_SET(0x7124, 0x1050); REG_SET(0x7128, 0x1050); REG_SET(0x712c, 0x1050); REG_SET(0x7130, 0x1050); REG_SET(0x7134, 0x1050); REG_SET(0x7138, 0x1050); REG_SET(0x713c, 0x1050);
		REG_SET(0x7140, 0x1050); REG_SET(0x7144, 0x1050); REG_SET(0x7148, 0x1050);
	}

	reg_bit_set(RTL837X_REG_HW_CONF, 0);

	// TODO: patch the PHYs

	// Re-enable PHY after configuration
	if (isRTL8373)
		phy_write(0xff,0x1f,0xa610,0x2058);
	else
		phy_write(0xf0,0x1f,0xa610,0x2058);

	// Enables MAC access
	// Set bits 0xc-0x14 of 0x632c to 0x1f8, see rtl8372_init
	// r632c:00000540 R632c-001f8540 // RTL8373: 001ff540
	reg_read_m(0x632c);
	if (isRTL8373)
		sfr_mask_data(1, 0x70, 0xf0); // The ports of the RTL8824
	else
		sfr_mask_data(1, 0x70, 0x80);
	sfr_mask_data(2, 0x10, 0x1f);
	reg_write_m(0x632c);

	delay(1000);

	handle_sfp();

	print_string("\r\nrtl8372_init done\r\n");
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
}


void bootloader(void)
{
	ticks = 0;
	sbuf_ptr = 0;

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

	// HW setup, serial, timer, external IRQs
	setup_clock();
	setup_serial();
	setup_timer0();
	setup_external_irqs();
	EA = 1; // Enable all IRQs

	// Set default for SFP pins so we can start up a module already inserted
	sfp_pins_last = 0x3; // signal LOS and no module inserted

	print_string("\r\nDetecting CPU");
	isRTL8373 = 0; // FIXME: See below
	reg_read_m(0x4);
	if (sfr_data[1] == 0x73) { // Register was 0x83730000
		print_string("\r\nRTL8373 detected");
		isRTL8373 = 1;
		rtl8224_enable();  // Power on the RTL8224
	}

#ifdef DEBUG
	// Reset seconds counter
	print_string("\r\nTIMER-TEST: \r\n");
	REG_SET(RTL837X_REG_SEC_COUNTER, 0x0);
	delay(100);
	print_reg(RTL837X_REG_SEC_COUNTER); write_char(' ');
	REG_SET(RTL837X_REG_SEC_COUNTER, 0x1);
	delay(100);
	print_reg(RTL837X_REG_SEC_COUNTER);
	REG_SET(RTL837X_REG_SEC_COUNTER, 0x2); write_char(' ');
	delay(100);
	print_reg(RTL837X_REG_SEC_COUNTER);
	REG_SET(RTL837X_REG_SEC_COUNTER, 0x3); write_char(' ');
	print_reg(RTL837X_REG_SEC_COUNTER);
#endif

	print_string("\r\nStarting up...\r\n");
	print_string("  Flash controller\r\n");
	flash_init(0);

	// Reset NIC
	reg_bit_set(0x24, 2);
	do {
		reg_read(0x24);
	} while (SFR_DATA_0 & 0x4);
	print_string("\r\nNIC reset");

	rtl8372_init();
	REG_SET(0x7f94, 0x0);	// BUG: Only for testing, otherwise: clear bits 0-3
	nic_setup();
	vlan_setup();

	was_offline = 1;

	setup_i2c();

	print_string(greeting);
	print_string("\r\nCPU detected: ");
	if (isRTL8373)
		print_string("RTL8373");
	else
		print_string("RTL8372");
	print_string("\r\nClock register: ");
	print_reg(0x6040);
	print_string("\r\nRegister 0x7b20/RTL837X_REG_SDS_MODES: ");
	print_reg(0x7b20);

	print_string("\r\nVerifying PHY settings:\n");
//	p031f.a610:2058 p041f.a610:2058  p051f.a610:2058  r4f3c:00000000 p061f.a610:2058 p071f.a610:2058 
	port_stats_print();

	print_string("\r\n> ");
	char l = sbuf_ptr;
	char line_ptr = l;
	char is_white = 1;
	while (1) {
		while (l != sbuf_ptr) {
			write_char(sbuf[l]);
			// Check whether there is a full line:
			if (sbuf[l] == '\n' || sbuf[l] == '\r') {
#ifdef DEBUG
				print_long(ticks);
#endif
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
/*					print_string("\r\n  THERE may be a command: ");
					print_short(i); print_string("\r\n");
					print_short(cmd_words_b[0]); print_string("\r\n");
					print_short(cmd_words_b[1]); print_string("\r\n");
					print_short(cmd_words_b[2]); print_string("\r\n");
					print_short(cmd_words_b[3]); print_string("\r\n");
*/
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
					if (cmd_compare(0, "stat")) {
						port_stats_print();
					}
					if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'r') {
						print_string("\r\nPRINT SECURITY REGISTERS\r\n");
						// The following will only show something else then 0xff if it was programmed for a managed switch
						flash_read_security(0x0001000, 40);
						flash_read_security(0x0002000, 40);
						flash_read_security(0x0003000, 40);
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
					if (cmd_compare(0, "port") && cmd_words_b[1] > 0) {
						print_string("\r\nPORT ");
						uint8_t p = sbuf[cmd_words_b[1]] - '1';
						print_byte(p);
						if (cmd_words_b[2] > 0 && cmd_compare(2, "2g5")) {
							print_string(" 2.5G\r\n");
							phy_set_mode(p, PHY_SPEED_2G5, 0, 0);
						}
						if (cmd_words_b[2] > 0 && cmd_compare(2, "1g")) {
							print_string(" 1G\r\n");
							phy_set_mode(p, PHY_SPEED_1G, 0, 0);
						}
						if (cmd_words_b[2] > 0 && cmd_compare(2, "auto")) {
							print_string(" AUTO\r\n");
							phy_set_mode(p, PHY_SPEED_AUTO, 0, 0);
						}
						if (cmd_words_b[2] > 0 && cmd_compare(2, "off")) {
							print_string(" OFF\r\n");
							phy_set_mode(p, PHY_OFF, 0, 0);
						}
					}
					if (cmd_compare(0, "l2")) {
						port_l2_learned();
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

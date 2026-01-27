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
#include "rtl837x_stp.h"
#include "rtl837x_igmp.h"
#include "dhcp.h"
#include "cmd_parser.h"
#include "uip/uipopt.h"
#include "uip/uip.h"
#include "uip/uip_arp.h"
#include "machine.h"


extern __code const struct machine machine;

extern __xdata uint16_t crc_value;
__xdata uint8_t crc_testbytes[10];
void crc16(__xdata uint8_t *v) __naked;

// Upload Firmware to 1M
#define FIRMWARE_UPLOAD_START 0x100000

// See setup_serial_timer1() for valid baudrate settings!
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

/* Derive divider for the system ticks
   TIMER2 can divide the F_CPU by 4 or 12.
   So the F_TICKS are in the range of:
   -  F_TIMER_DIV4_OVERFLOW = F_SYS /  DIV4 / 1..65536 = 125MHz /  4 / 1..65536 = 31.25 MHz .. 476.8 Hz
   - T_TIMER_DIV12_OVERFLOW = F_SYS / DIV12 / 1..65536 = 125MHz / 12 / 1..65536 = 10.42 MHz .. 158.9 Hz
   Selecting dividor 12 settings to get lowest timer tick posiable which is already high.
*/
#define SYS_TICK_HZ 200

#define TIMER2_DIV (CLOCK_HZ / 12 / SYS_TICK_HZ)
#if TIMER2_DIV > 0xFFFF
#error "SYS_TICK_HZ to low, must be >= 159"
#endif
#define SYSTICK_TIMER2_VALUE (0x10000 - TIMER2_DIV)

__xdata uint8_t idle_ready;

__code uint8_t ownIP[] = { 192, 168, 2, 2 };
__code struct uip_eth_addr uip_ethaddr = {{ 0x1c, 0x2a, 0xa3, 0x23, 0x00, 0x02 }};
__code uint8_t gatewayIP[] = { 192, 168, 2, 22};
__code uint8_t netmask[] = { 255, 255, 255, 0};

volatile __xdata uint32_t ticks;
volatile __xdata uint8_t sec_counter;
volatile __xdata uint16_t sleep_ticks;
__xdata uint8_t stp_clock;
extern __xdata struct dhcp_state dhcp_state;

#define STP_TICK_DIVIDER 3


// Buffer for serial input, SBUF_SIZE must be power of 2 < 256
__xdata volatile uint8_t sbuf_ptr;
__xdata uint8_t sbuf[SBUF_SIZE];
__xdata uint8_t sfr_data[4];

extern __xdata uint8_t gpio_last_value[8];

extern __xdata struct flash_region_t flash_region;

__code uint8_t * __code greeting = "\nA minimal prompt to explore the RTL8372:\n";
__code uint8_t * __code hex = "0123456789abcdef";

__xdata uint8_t flash_buf[512];

// NIC buffers for packet RX/TX
__xdata uint8_t rx_headers[16]; // Packet header(s) on RX
__xdata uint8_t uip_buf[UIP_CONF_BUFFER_SIZE+2];

__xdata uint16_t rx_packet_vlan;
__xdata uint16_t management_vlan;
__xdata uint8_t tx_seq;

__xdata uint8_t stpEnabled;

__code uint16_t bit_mask[16] = {
	0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
	0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000
};


__xdata uint8_t linkbits_last[4];
__xdata uint8_t linkbits_last_p89;
__xdata uint8_t sfp_pins_last;
__xdata char sfp_module_vendor[2][17];
__xdata char sfp_module_model[2][17];
__xdata char sfp_module_serial[2][17];
__xdata uint8_t sfp_options[2];
__sbit tx_buf_empty;

#define ETHERTYPE_OFFSET (12 + VLAN_TAG_SIZE + RTL_TAG_SIZE)

void isr_timer0(void) __interrupt(1)
{
}


// Timer2: Handle SYS_TICK
void isr_timer2(void) __interrupt(5)
{
	ticks++;
	if (sleep_ticks > 0)
		sleep_ticks--;
	sec_counter++;

	// Clear TF2 & EXF2 by software
	T2CON &= ~0xC0;
}


void isr_serial(void) __interrupt(4)
{
	if (RI == 1) {
		RI = 0;
		sbuf[sbuf_ptr] = SBUF;
		sbuf_ptr = (sbuf_ptr + 1) & (SBUF_SIZE - 1);
	}
	if (TI == 1) {
		TI = 0;
		tx_buf_empty = 1;
	}
}


void write_char(char c)
{
	do {
	} while (tx_buf_empty == 0);
	if (c =='\n') {
		tx_buf_empty = 0;
		SBUF = '\r';
		do {
		} while (tx_buf_empty == 0);
	}
	tx_buf_empty = 0;
	SBUF = c;
}


void itoa(uint8_t v)
{
	uint8_t t = (v / 100);
	// when print_zeros is not zero, we know that a non-zero number has printed.
	// That have to print all the next numbers.
	uint8_t print_zeros = t;
	if (print_zeros)
		write_char('0' + t);
	t = (v / 10) % 10;
	print_zeros |= t;
	if (print_zeros)
		write_char('0' + t);
	write_char('0' + (v % 10));
}


void print_string(__code char *p)
{
	while (*p)
		write_char(*p++);
}

void print_string_x(__xdata char *p)
{
	while (*p)
		write_char(*p++);
}


void memcpy(__xdata void * __xdata dst, __xdata const void * __xdata src, uint16_t len)
{
	__xdata uint8_t *d = dst;
	__xdata const uint8_t *s = src;
	while (len--)
		*d++ = *s++;
}

void memcpyc(register __xdata uint8_t *dst, register __code uint8_t *src, register uint16_t len)
{
	while (len--)
		*dst++ = *src++;
}


void memset(register __xdata uint8_t *dst, register __xdata uint8_t v, register uint8_t len)
{
	while (len--)
		*dst++ = v;
}

uint16_t strtox(register __xdata uint8_t *dst, register __code const char *s)
{
	__xdata uint8_t *b = dst;
	while (*s)
		*dst++ = *s++;
	*dst = 0;
	return dst - b;
}

uint16_t strlen(register __code const char *s)
{
	uint16_t l = 0;
	while (s[l])
		l++;
	return l;
}


uint16_t strlen_x(register __xdata const char *s)
{
	uint16_t l = 0;
	while (s[l])
		l++;
	return l;
}


void print_short(uint16_t a)
{
	print_string("0x");
	for (signed char i = 12; i >= 0; i -= 4) {
		write_char(hex[(a >> i) & 0xf]);
	}
}


void print_long(__xdata uint32_t a)
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

// Timer2: handles system tick.
void setup_timer2(void)
{
	T2CON = 0x00; // Timer2: Mode 16-bit timer with auto-reload, disable the timer.

	// Timer 2 clock select F_SYS / 12;
	// T2M = 0 uses clk/12;
	CKCON &= ~0x20;

	// The RCAP2 registers contain the high/low byte that is loaded into
	// timer2 when T2 overflows to 0x10000
	RCAP2_U16 = SYSTICK_TIMER2_VALUE;

	T2CON |= 0x04; // Timer2: Enable

	// IP |= 0x20; // TEST: Make Timer 2 interrupt as high priority.
	ET2 = 1; // Enable Timer2 interrupt.


}


void reg_read(uint16_t reg_addr)
{
	SFR_REG_ADDR_U16 = reg_addr;
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
	SFR_REG_ADDR_U16 = reg_addr;
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
	SFR_REG_ADDR_U16 = reg_addr;
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
	SFR_REG_ADDR_U16 = reg_addr;
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
 * This masks the sfr data fields, first &-ing with ~mask, then setting the bits in set
 */
void sfr_mask_data(uint8_t n, uint8_t mask, uint8_t set)
{
	uint8_t b = sfr_data[3-n];
	b &= ~mask;
	b |= set;
	sfr_data[3-n] = b;
}

/*
 * This zeros all the sfr data fields
 */
void sfr_set_zero(void) {
	uint8_t idx = 4;
	while (idx) {
		idx -= 1;
		sfr_data[idx] = 0;
	}
}


/*
 * Create 32 random number in sfr_data
 */
void get_random_32(void)
{
	// In order to get a new random numner, this bit has to be set each time!
	reg_bit_set(RTL837X_RLDP_RLPP, RLDP_RND_EN);
	reg_read_m(RTL837X_RAND_NUM0);
}


/*
 * Transfer Network Interface RX data from the ASIC to the 8051 XMEM
 * data will be stored in the rx_header structure
 * len is the length of data to be transferred
 */
void nic_rx_header(uint16_t ring_ptr)
{
	uint16_t buffer = (uint16_t) &rx_headers[0];
	SFR_NIC_DATA_U16LE = buffer;
	SFR_NIC_RING_U16LE = ring_ptr;
	SFR_NIC_CTRL = 1;
	do { } while (SFR_NIC_CTRL != 0);
}


/*
 * Transfer Network Interface RX data from the ASIC to the 8051 XMEM
 * the description of the packet must be in the rx_headers data structure
 * data will be returned in the xmem buffer points to
 * ring_ptr is the current position of the RX Ring on the ASIC side
 */
void nic_rx_packet(register uint16_t buffer, register uint16_t ring_ptr)
{
	SFR_NIC_DATA_U16LE = buffer;
	SFR_NIC_RING_U16LE = ring_ptr;

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


/*
 * Transfers data in XMEM to the ASIC for transmission by the nic
 */
void nic_tx_packet(uint16_t ring_ptr)
{
//	uint16_t buffer = (uint16_t) tx_buf;
	uint16_t buffer = (uint16_t) uip_buf + VLAN_TAG_SIZE;
	SFR_NIC_DATA_U16LE = buffer;
	
	ring_ptr <<= 3;
	ring_ptr |= 0x8000;
	SFR_NIC_RING_U16LE = ring_ptr;
	
	uint16_t len = (((uint16_t)uip_buf[VLAN_TAG_SIZE + 5]) << 8) | uip_buf[VLAN_TAG_SIZE + 4];
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
	for (uint8_t i=0; i < 4; i++) {
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
	SFR_DATA_U16 = v;
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
	for (uint8_t i = 0; i < 4; i++) {
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
	for (uint8_t i = 0; i < 4; i++)
		dest[i] = source[i];
}


void read_reg_timer(uint32_t * tmr)
{
	uint8_t * val = (uint8_t *)tmr;
	SFR_REG_ADDR_U16 = RTL837X_REG_SEC_COUNTER;
	SFR_EXEC_GO = SFR_EXEC_READ_REG;
	do {
	} while (SFR_EXEC_STATUS != 0);
	*val++ = SFR_DATA_0;
	*val++ = SFR_DATA_8;
	*val++ = SFR_DATA_16;
	*val = SFR_DATA_24;
}


void sds_config_mac(uint8_t sds, uint8_t mode)
{
	reg_read_m(RTL837X_REG_SDS_MODES);
	sfr_data[0] = 0;
	sfr_data[1] = 0;
	switch (sds) {
	case 0:
		sfr_mask_data(0, 0x1f, mode);
		break;
	case 1:
		sfr_mask_data(0, 0xe0, mode << 5);
		sfr_mask_data(1, 0x03, mode >> 3);
		break;
	case 2:
		sfr_mask_data(1, 0xfc, 0x02 << 2);
	}
	if (machine.isRTL8373) // Set 3rd SERDES Mode to 0x2 for RTL8224
		sfr_mask_data(1, 0xfc, 0x02 << 2);
	else
		sfr_data[2] &= 0x03;
	reg_write_m(RTL837X_REG_SDS_MODES);
	print_string("\nRTL837X_REG_SDS_MODES: ");
	print_reg(RTL837X_REG_SDS_MODES);
	print_string("\n");
}


// Delay for given number of ticks without doing housekeeping
void delay(uint16_t t)
{
	sleep_ticks = t;
	while (sleep_ticks > 0)
		PCON |= 1;
}


/*
 * Configure the SerDes of the SoC for a particular mode
 * to connect to an SFP module or a PHY
 * Valid modes are SDS_10GR, SDS_QXGMII, SDS_HISGMII, SDS_HSG, SDS_SGMII and SDS_1000BX_FIBER
 * The SerDes ID may be 0 or 1 for RTL8272 and 0-2 for RTL8373
 */
void sds_config(uint8_t sds, uint8_t mode)
{
	print_string("sds_config sds: "); print_byte(sds); print_string(", mode: "); print_byte(mode); write_char('\n');
	sds_config_mac(sds, mode);

	if (mode == SDS_10GR || mode == SDS_QXGMII) // 10G Fiber, 10G connection to RTL8224
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
	case SDS_QXGMII:
		v = 0x0200;
		page = 0x2e;
		break;
	default:
		print_string("Error in SDS Mode\n");
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
		sds_write_v(sds, page, 0x15, 0xe7c1); // Q002815:e7f1 BUG !
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
	if (mode != SDS_QXGMII)
		sds_write_v(sds, 0x06, 0x1f, 0x2100); // Q00061f:2100

	if (sds == 0 && mode == SDS_1000BX_FIBER) {
		sds_write_v(sds, 0x02, 0x04, 0x0020); 	// Q000204:0020
		sds_write_v(sds, 0x00, 0x02, 0x73d0); 	// Q000002:73d0
		sds_write_v(sds, 0x00, 0x04, 0x074d); 	// Q000004:074d
		sds_write_v(sds, 0x20, 0x04, 0x0000); 	// Q002000:0000
		sds_write_v(sds, 0x1f, 0x00, 0x0000); 	// Q001f00:0000
	}
}


/*
 * Read a register of the EEPROM via I2C
 */
uint8_t sfp_read_reg(uint8_t slot, uint8_t reg)
{
	if (reg & 0x80) {	// Configure SFP readings address (0x51) as I2C device address
		reg &= 0x7f;
		REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00, 0x1 << (I2C_MEM_ADDR_WIDTH-16) | 1,  0x51 >> 5, (0x51 << 3) & 0xff);
	} else {
		REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00, 0x1 << (I2C_MEM_ADDR_WIDTH-16) | 1,  0x50 >> 5, (0x50 << 3) & 0xff);
	}

	reg_read_m(RTL837X_REG_I2C_CTRL);
	sfr_mask_data(1, 0xfc, machine.sfp_port[slot].i2c == 0 ? SCL_PIN << 5 | SDA_PIN_0 << 2 : SCL_PIN << 5 | SDA_PIN_1 << 2 );
	reg_write_m(RTL837X_REG_I2C_CTRL);

	REG_WRITE(RTL837X_REG_I2C_IN, 0, 0, 0, reg);

	// Execute I2C Read
	reg_bit_set(RTL837X_REG_I2C_CTRL, 0);

	// Wait for execution to finish
	do {
		reg_read_m(RTL837X_REG_I2C_CTRL);
	} while (sfr_data[3] & 0x1);

	reg_read_m(RTL837X_REG_I2C_OUT);
	return sfr_data[3];
}


/*
 * Adds TX Header to uip_buf and calls nic_tx_packet to send the packet
 * over the wire
 */
void tcpip_output(void)
{
	// Add TX-TAG
	uip_buf[VLAN_TAG_SIZE] = tx_seq++;
	uip_buf[VLAN_TAG_SIZE + 1] = 0x07;    // Enable all checksums
	uip_buf[VLAN_TAG_SIZE + 5] = uip_len >> 8;
	uip_buf[VLAN_TAG_SIZE + 4] = uip_len;
	uip_buf[VLAN_TAG_SIZE + 2] = uip_buf[VLAN_TAG_SIZE + 3] = 0;
	uip_buf[VLAN_TAG_SIZE + 6] = uip_buf[VLAN_TAG_SIZE + 7] = 0;

	reg_read_m(RTL837X_REG_CPU_TX_CURR_PKT);
	uint16_t ring_ptr = ((uint16_t)sfr_data[2]) << 8;
	ring_ptr |= sfr_data[3];

#ifdef RXTXDBG
	print_string("TX: \n");
	for (uint8_t i = 0; i < 120; i++) {
		print_byte(uip_buf[i]);
		write_char(' ');
	}
	write_char('\n');
#endif

	// Move data over from xmem buffer to ASIC side using DMA
	nic_tx_packet(ring_ptr);

	// New position of the ring-pointer on the NIC-side indicates number of bytes transmitted
	reg_read_m(RTL837X_REG_NIC_TX_CURR_PKT);

	// Do actual TX of data on ASIC side
	REG_SET(RTL837X_REG_NIC_TXCMD, 1);
}


void handle_rx(void)
{
	// Check the amount of data available on the NIC/ASIC side
	reg_read_m(RTL837X_REG_NIC_RX_BUFF_DATA);
	if (sfr_data[2] != 0 || sfr_data[3] != 0) {
		reg_read_m(RTL837X_REG_CPU_RX_CURR_PKT);
		uint16_t ring_ptr = ((uint16_t)sfr_data[2]) << 8;
		ring_ptr |= sfr_data[3];
		ring_ptr <<= 3;
		nic_rx_header(ring_ptr);
#ifdef RXTXDBG
		__xdata uint8_t *ptr = rx_headers;
		print_string("RX on port "); print_byte(rx_headers[3] & 0xf);
		print_string(": ");
		for (uint8_t i = 0; i < 8; i++) {
			print_byte(*ptr++);
			write_char(' ');
		}
#endif
		nic_rx_packet((uint16_t) &uip_buf[0], ring_ptr + 8);

#ifdef RXTXDBG
		print_string("\n<< ");
		ptr = &uip_buf[0];
		for (uint8_t i = 0; i < 80; i++) {
			print_byte(*ptr++);
			write_char(' ');
		}
#endif
		REG_SET(RTL837X_REG_NIC_RXCMD, 1);
		uip_len = (((uint16_t)rx_headers[5]) << 8) | rx_headers[4];

		// Retrieve VLAN from VLAN-tag
		rx_packet_vlan = uip_buf[2 * sizeof (struct uip_eth_addr) + RTL_TAG_SIZE + 2] & 0xf;
		rx_packet_vlan <<= 8;
		rx_packet_vlan |= uip_buf[2 * sizeof (struct uip_eth_addr) + RTL_TAG_SIZE + 3];
#ifdef RXTXDBG
		print_string(" RX-VLAN: "); print_short(rx_packet_vlan); write_char('\n');
		print_string(" RX dst: "); print_byte(uip_buf[0]); print_byte(uip_buf[1]); print_byte(uip_buf[2]);
		print_byte(uip_buf[3]); print_byte(uip_buf[4]); print_byte(uip_buf[5]); write_char('\n');
#endif
		if (stpEnabled && uip_buf[0] == 0x01 && uip_buf[1] == 0x80 && uip_buf[2] == 0xc2 // STP packet?
			&& uip_buf[3] == 0x00 && uip_buf[4] == 0x00 && uip_buf[5] == 0x00) {
			stp_in();
			if (uip_len) {
				print_string("STP TX\n");
				tcpip_output();
			}
		} else if (uip_buf[0] == 0x01 && uip_buf[1] == 0x00 && uip_buf[2] == 0x5e // IPv4-MC packet?
			&& uip_buf[3] == 0x00 && uip_buf[4] == 0x00 && uip_buf[5] == 0x16) {
			igmp_packet_handler();
			if (uip_len) {
				tcpip_output();
			}
		} else if (uip_buf[ETHERTYPE_OFFSET] == 0x08 && uip_buf[ETHERTYPE_OFFSET + 1] == 0x06) { // ARP?
			uip_arp_arpin();
			if (uip_len) {
			    tcpip_output();
			}
		} else if (uip_buf[ETHERTYPE_OFFSET] == 0x08 && uip_buf[ETHERTYPE_OFFSET + 1] == 0x00) { // TCP?
			if (!management_vlan || management_vlan == rx_packet_vlan) {
				uip_arp_ipin();	// Learn MAC addresses in TCP packets
				uip_input();
				if (uip_len) {
					// Add ethernet frame
					uip_arp_out();
					tcpip_output();
				}
			}
		} else {
#ifdef RXTXDBG
			print_string("Unknown RX on port "); print_byte(rx_headers[3] & 0xf); write_char('\n');
#endif
		}
	}
}


void handle_tx(void)
{
	for(uint8_t i = 0; i < UIP_CONNS; i++) {
		uip_periodic(i);
		if(uip_len > 0) {
#ifdef RXTXDBG
			write_char('.'); print_short(i);
#endif
			uip_arp_out();
			tcpip_output();
		}
	}
	for(uint8_t i = 0; i < UIP_UDP_CONNS; i++) {
		uip_udp_periodic(i);
		if(uip_len > 0) {
			uip_arp_out();
			tcpip_output();
		}
	}
}


static inline uint8_t sfp_rate_to_sds_config(register uint8_t rate)
{
	if (rate == 0xd)
		return SDS_1000BX_FIBER;
	if (rate == 0x1f)  // Ethernet 2.5 GBit
		return SDS_HSG;
	if (rate > 0x65 && rate < 0x70)
		return SDS_10GR;
	return 0xff;
}


void sfp_print_info(uint8_t sfp)
{
	// This loops over the Vendor-name, Vendor OUI, Vendor PN and Vendor rev ASCII fields
	for (uint8_t i = 20; i < 60; i++) {
		if (i >= 36 && i < 40) // Skip Non-ASCII codes
			continue;
		uint8_t c = sfp_read_reg(sfp, i);
		if (c)
			write_char(c);
	}
	print_string("\n");
}


void sfp_get_info(uint8_t sfp)
{
	for (uint8_t i = 20; i < 36; i++)
		sfp_module_vendor[sfp][i-20] = sfp_read_reg(sfp, i);
	sfp_module_vendor[sfp][16] = '\0';
	for (uint8_t i = 40; i < 56; i++)
		sfp_module_model[sfp][i-40] = sfp_read_reg(sfp, i);
	sfp_module_model[sfp][16] = '\0';
	for (uint8_t i = 68; i < 84; i++)
		sfp_module_serial[sfp][i-68] = sfp_read_reg(sfp, i);
	sfp_module_serial[sfp][16] = '\0';
}


bool gpio_pin_test(uint8_t pin)
{
	reg_read_m(RTL837X_REG_GPIO_00_31_INPUT + (pin > 31 ? 4 : 0));
	return sfr_data[3-((pin >> 3) & 3)] & (1 << (pin & 7));
}


void handle_sfp(void)
{
	for (uint8_t sfp = 0; sfp < machine.n_sfp; sfp++) {
		if (!gpio_pin_test(machine.sfp_port[sfp].pin_detect)) {
			if (sfp_pins_last & (0x1 << (sfp << 2))) {
				sfp_pins_last &= ~(0x01 << (sfp << 2));
				print_string("\n<MODULE INSERTED>  Slot: "); write_char('1' + sfp);
				// Read Reg 11: Encoding, see SFF-8472 and SFF-8024
				// Read Reg 12: Signalling rate (including overhead) in 100Mbit: 0xd: 1Gbit, 0x67:10Gbit
				delay(100); // Delay, because some modules need time to wake up
				uint8_t rate = sfp_read_reg(sfp, 12);
				print_string("  Rate: "); print_byte(rate);  // Normally 1, but 0 for DAC, can be ignored?
				print_string("  Encoding: "); print_byte(sfp_read_reg(sfp, 11));
				print_string("  Module: "); sfp_print_info(sfp);
				print_string("\n");
				sfp_options[sfp] = sfp_read_reg(sfp, 92);
				sfp_get_info(sfp);
				sds_config(machine.sfp_port[sfp].sds, sfp_rate_to_sds_config(rate));
			}
		} else {
			if (!(sfp_pins_last & (0x1 << (sfp << 2)))) {
				sfp_pins_last |= 0x01 << (sfp << 2);
				print_string("\n<MODULE REMOVED>  Slot: "); write_char('1' + sfp); write_char('\n');
			}
		}

		if (!gpio_pin_test(machine.sfp_port[sfp].pin_los)) {
			if (sfp_pins_last & (0x2 << (sfp << 2))) { // 0x2 0x08
				sfp_pins_last &= ~(0x02 << (sfp << 2));
				print_string("\n<SFP-RX OK>  Slot: "); write_char('1' + sfp); write_char('\n');
			}
		} else {
			if (!(sfp_pins_last & 0x2 << (sfp << 2))) {
				sfp_pins_last |= 0x02 << (sfp << 2);
				print_string("\n<SFP-RX LOS>  Slot: "); write_char('1' + sfp); write_char('\n');
			}
		}
	}
}

//
// An idle function that sleeps for 1 tick and does all the house-keeping
//
void idle(void)
{
	PCON |= 1;
	if (sec_counter >= SYS_TICK_HZ) {
		sec_counter -= SYS_TICK_HZ;
		reg_read_m(RTL837X_REG_SEC_COUNTER);
		uint8_t v = sfr_data[3];
#ifdef DEBUG
		print_string("  Tick counter: "); print_long(ticks); write_char('\n');
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
		print_sfr_data();
		write_char('\n');
#endif
	}

	// Check for Link changes
	reg_read_m(RTL837X_REG_LINKS_89);
	__xdata uint8_t linkbits_p89 = sfr_data[3];

	reg_read_m(RTL837X_REG_LINKS);
	if (cmp_4(sfr_data, linkbits_last) || (linkbits_p89 != linkbits_last_p89)) {
		print_string("\n<new link: ");
		print_byte(linkbits_p89); print_byte(sfr_data[0]); print_byte(sfr_data[1]);
		print_byte(sfr_data[2]); print_byte(sfr_data[3]);
		print_string(", was ");
		print_byte(linkbits_last_p89); print_byte(linkbits_last[0]); print_byte(linkbits_last[1]);
		print_byte(linkbits_last[2]); print_byte(linkbits_last[3]);
		print_string(">\n");
		linkbits_last_p89 = linkbits_p89;
		if (!machine.isRTL8373 && machine.n_sfp != 2) {
			uint8_t p5 = sfr_data[2] >> 4;
			uint8_t p5_last = linkbits_last[2] >> 4;
			cpy_4(linkbits_last, sfr_data);
			// Handle link change of the RTL8221 PHY, adjust SDS mode
			if (p5_last != p5) {
				if (p5 == 0x5) // 2.5GBit Mode
					sds_config(0, SDS_HISGMII);
				else if (p5 == 0x2) // 1GBit
					sds_config(0, SDS_SGMII);
			}
		} else {
			cpy_4(linkbits_last, sfr_data);
		}
	}

	// Check for changes with SFP modules
	handle_sfp();

	/* Button pressed on KL-8xhm-x2:
	reg_read(RTL837X_REG_GPIO_32_63_INPUT);
	if (!(sfr_data[2] & 0x40))
		print_string("Button pressed\n");
	*/
	// Check new Packets RX
	handle_rx();
	// Check UIP for packets to transmit
	handle_tx();
	// If STP protocol enabled, decrease STP timers to trigger actions
	if (stpEnabled) {
		if (!stp_clock) {
			stp_clock = STP_TICK_DIVIDER;
			stp_timers();
		} else {
			stp_clock--;
		}
	}
	// Check whether a command is waiting in the cmd_buffer and execute
	if (cmd_available) {
		cmd_available = 0;
		if (!cmd_tokenize())
			cmd_parser();
		print_string("\n> ");
	}
}


// Sleep the given number of ticks and perform idle tasks if initialized
void sleep(uint16_t t)
{
	sleep_ticks = t;
	while (sleep_ticks > 0) {
		if (idle_ready)
			idle();
		else
			PCON |= 1;
	}
}


void reset_chip(void)
{
	REG_SET(RTL837X_REG_RESET, 1);
	while(1);
}


void setup_external_irqs(void)
{
	REG_SET(0x5f84, 0x42);
	REG_SET(0x5f34, 0x3ff);

//	EX0 = 1;	// Enable external IRQ 0 (Link-change)
	EX0 = 0;
	IT0 = 1;	// External IRQ on falling edge

	EX1 = 1;	// External IRQ 1 enable
	EX2 = 1;	// External IRQ 2 enable: bit EIE.0
	EX3 = 1;	// External IRQ 3 enable: bit EIE.1
	PX3 = 1;	// Set EIP.1 = 1: External IRQ 3 set to high priority
}


void rtl8224_enable(void)
{
	// Set Pin 4 low
	reg_bit_clear(RTL837X_REG_GPIO_32_63_OUTPUT, 4);
	// Configure Pin as output
	reg_bit_set(RTL837X_REG_GPIO_32_63_DIRECTION, 4);
	delay(100);
	// Set pin 4 high
	reg_bit_set(RTL837X_REG_GPIO_32_63_OUTPUT, 4);
	delay(500);
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
	// Bit 8 is set in managed mode 125MHz to use fast SPI mode
	sfr_mask_data(1, 0, 0x01);
	reg_write_m(RTL837X_REG_HW_CONF);

	// Enable serial interface, set bit 0
	reg_read_m(RTL837X_PIN_MUX_1);
	sfr_mask_data(0, 0x1, 0x1);
	reg_write_m(RTL837X_PIN_MUX_1);
}


/*
 * Write a register reg of multipule phys, using a mask to select them, in page page
 * Data to be written is in v
 */
void phy_write_mask(uint16_t phy_mask, uint8_t dev_id, uint16_t reg, uint16_t v)
{
#ifdef REGDBG
	print_string("P"); print_byte(phy_mask>>8); print_byte(phy_mask); print_byte(dev_id); write_char('.'); print_byte(reg>>8); print_byte(reg); write_char(':');
	print_byte(v>>8); print_byte(v); write_char(' ');
#endif
	SFR_DATA_U16 = v;			    // SFR_A6, SFR_A7
	SFR_SMI_PHYMASK = phy_mask;		// SFR_C5
	SFR_SMI_REG_U16 = reg;			// SFR_C2, SFR_C3
	SFR_SMI_DEV = (phy_mask >> 8) | dev_id  << 3 | 2; // SFR_C4: bit 2 can also be set for some option
	SFR_EXEC_GO = SFR_EXEC_WRITE_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);
}

/*
 * Write a register reg of phy, using a mask to select them, in page page
 * Data to be written is in v
 */
void phy_write(uint8_t phy_id, uint8_t dev_id, uint16_t reg, uint16_t v)
{
	uint16_t phy_mask =  bit_mask[phy_id];
#ifdef REGDBG
	print_string("P"); print_byte(phy_mask>>8); print_byte(phy_mask); print_byte(dev_id); write_char('.'); print_byte(reg>>8); print_byte(reg); write_char(':');
	print_byte(v>>8); print_byte(v); write_char(' ');
#endif
	SFR_DATA_U16 = v;			    // SFR_A6, SFR_A7
	SFR_SMI_PHYMASK = phy_mask;		// SFR_C5
	SFR_SMI_REG_U16 = reg;			// SFR_C2, SFR_C3
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
	SFR_SMI_REG_U16 = reg;		// c2, c2

	SFR_SMI_PHY = phy_id;		// a5
	SFR_SMI_DEV = dev_id << 3 | 2;	// c4

	SFR_EXEC_GO = SFR_EXEC_READ_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);
#ifdef REGDBG
	print_byte(SFR_DATA_8); print_byte(SFR_DATA_0); write_char(' ');
#endif
}

/*
 * Modify a register reg of phy phy_id, in page page
 * Set: bit mask of bits to set.
 * Mask: bit mask of bits to clear.

 * Note: We assume that the registers `SFR_SMI_REG_U16`, `SFR_SMI_PHY` and `SFR_SMI_DEV` 
 * keep there value, and dont have to be rewritten everytime.
 */
void phy_modify(uint8_t phy_id, uint8_t dev_id, uint16_t reg, uint16_t mask, uint16_t set)
{
	uint8_t smi_phy = dev_id << 3 | 2;

	// Read the data
	SFR_SMI_REG_U16 = reg;		// c2, c2
	SFR_SMI_PHY = phy_id;		// a5
	SFR_SMI_DEV = smi_phy;		// c4
	SFR_EXEC_GO = SFR_EXEC_READ_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);

	// Modify the reed data.
	// TODO: Check if we directly can modify SFR register directly.
	uint16_t data = SFR_DATA_U16 & ~(mask);
	data |= set;

	uint16_t phy_mask = bit_mask[phy_id];

	// Write it back
	SFR_SMI_REG_U16 = reg;
	SFR_DATA_U16 = data;
	SFR_SMI_PHYMASK = phy_mask;		// SFR_C5
	SFR_SMI_DEV = smi_phy | (phy_mask >> 8);
	SFR_EXEC_GO = SFR_EXEC_WRITE_SMI;
	do {
	} while (SFR_EXEC_STATUS != 0);
}

void nic_setup(void)
{
	// Enable NIC
	// r6040:00000100 R6040-00001100
	reg_bit_set(RTL837X_REG_HW_CONF, 0xc);

	// This sets the size of the RX buffer, the filling level is in 0x7874
	// R7848-000004ff
	REG_SET(RTL837X_REG_NIC_RXBUFF_RX, 0x4ff);

	// R7844-000007fe
	REG_SET(RTL837X_REG_NIC_BUFFSIZE_TX, 0x7fe);

	// Configure NIC RX to receive various types of packets
	// RTL837X_REG_RX_CTRL: Set bits 24-31 to 0x4, clear bits 16/17
	reg_read_m(RTL837X_REG_RX_CTRL);
	sfr_mask_data(3, 0xff, 0x04);
	sfr_mask_data(2, 0x03, 0);
	reg_write_m(RTL837X_REG_RX_CTRL);

	// Enable NIC TX (set bit 0)
	reg_bit_set(RTL837X_REG_TX_CTRL, 0);

	// Enable NIC RX (set bit 0)
	reg_bit_set(RTL837X_REG_RX_CTRL, 0);

	// Drop packets with invalid CRC
	reg_bit_clear(RTL837X_REG_RX_CTRL, 2);

	// R603c-00000200
	// CPU-port is CPU-Tag aware (bit 9)
	REG_SET(RTL837X_REG_CPU_TAG_AWARE_PMASK, 0x200);

	// Insert CPU-tag for internally received packets (bit 0), MODE is 0, i.e. ALL packets (bits 8-9)
	reg_read_m(RTL837X_REG_CPU_TAG);
	sfr_mask_data(0, 1, 1);
	sfr_mask_data(1, 3, 0);
	reg_write_m(RTL837X_REG_CPU_TAG);

	// Force MAC mode of the CPU port (port 9)
	// r6368:00000194 R6368-00000197
	reg_read_m(RTL837X_REG_MAC_FORCE_MODE + 9 * 4);
	sfr_mask_data(0, 0, 3); // Set bits 0, 1: Force link
	reg_write_m(RTL837X_REG_MAC_FORCE_MODE+ 9 * 4);

	// Sequence number of TX packets
	tx_seq = 0;
}


/*
 * Configure the PHY-Side of the SDS-SDS link between SoC and PHY
 */
void sds_init(void)
{
/*
	p001e.000d:9535 R02f8-00009535 R02f4-0000953a P000001.1e00000d:953a
	p001e.000d:953a p001e.000d:953a R02f8-0000953a R02f4-00009530 P000001.1e00000d:9530

	RTL8373:
	p001e.000d:0010 R02f8-00000010 R02f4-0000001a P000001.1e00000d:b7fe
	p001e.000d:0010 p001e.000d:0010	R02f8-00000010 R02f4-00000010 P000001.1e00000d:b7fe
*/
	phy_read(0, 0x1e, 0xd);
	uint16_t pval = SFR_DATA_U16;

	// PHY Initialization:
	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);
	delay(20);

	pval &= 0xfff0;
	pval |= 0x0a;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);
	delay(10);

	phy_write_mask(0x1, 0x1e, 0xd, pval);

	phy_read(0, 0x1e, 0xd);
	pval = SFR_DATA_U16;

	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);

	pval &= 0xfff0;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);

	phy_write_mask(0x1, 0x1e, 0xd, pval);
}


void led_config_9xh(void)
{
	// r65d8:3ffbedff R65d8-3ffbedff
	reg_bit_set(0x65d8, 0x1d);

	//  r6520:0021fdb0 R6520-0021e7b0 r6520:0021e7b0 R6520-0021e6b0
	reg_read_m(RTL837X_REG_LED_MODE);
	sfr_mask_data(1, 0x1f, 0x6);
	sfr_mask_data(0, 0xe0, 0xa0);
	reg_write_m(RTL837X_REG_LED_MODE);

	// Set LED blink rate to slow during booting
	set_sys_led_state(SYS_LED_SLOW);

	// Disable RLDP (Realtek Loop Detection Protocol) LEDs on loop detection
	reg_read_m(RTL837X_REG_LED_RLDP_1);
	sfr_mask_data(0, 0, 0x3);
	reg_write_m(RTL837X_REG_LED_RLDP_1);
	// Configure LED group for RLDP per port
	REG_SET(RTL837X_REG_LED_RLDP_2, 0xffffffff);	// Ports 0-7
	REG_SET(RTL837X_REG_LED_RLDP_3, 0x0000000f);	// Port 8

	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 29);
	reg_bit_clear(RTL837X_REG_LED_GLB_IO_EN, 27);

	// GPIO 27 is LED
	reg_bit_set(RTL837X_PIN_MUX_0, 27);

	// Configure LED_SET_0, ledid 0/1
	REG_SET(RTL837X_REG_LED1_0_SET0, 0x0041017f);

	// Configure LED_SET_0 ledid 2
	REG_SET(RTL837X_REG_LED3_2_SET0, 0x01410044);

	// r6528:00000000 R6528-0000000f
	reg_read_m(RTL837X_REG_LED3_0_SET1);
	sfr_mask_data(0, 0x0f, 0x0f);
	reg_write_m(RTL837X_REG_LED3_0_SET1);
}

void set_sys_led_state(uint8_t state)
{
	reg_read_m(RTL837X_REG_LED_MODE);
	sfr_mask_data(2, 0x03, state);
	reg_write_m(RTL837X_REG_LED_MODE);
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

	// Disable RLDP (Realtek Loop Detection Protocol) LEDs on loop detection
	reg_read_m(RTL837X_REG_LED_RLDP_1);
	sfr_mask_data(0, 0x03, 0);
	reg_write_m(RTL837X_REG_LED_RLDP_1);
	// Configure LED group for RLDP per port
	REG_SET(RTL837X_REG_LED_RLDP_2, 0xffffffff);	// Ports 0-7
	REG_SET(RTL837X_REG_LED_RLDP_3, 0x0000000f);	// Port 8

	reg_bit_set(RTL837X_REG_LED_GLB_IO_EN, 29);
	reg_bit_clear(RTL837X_REG_LED_GLB_IO_EN, 27);

	// Configure GPIO for LEDs 27-29
	if (machine.n_sfp == 2) {
		reg_bit_set(RTL837X_PIN_MUX_0, 27);
		reg_bit_clear(RTL837X_PIN_MUX_0, 28);
		reg_bit_set(RTL837X_PIN_MUX_0, 29);
	} else {
		reg_bit_set(RTL837X_PIN_MUX_0, 27);
		reg_bit_set(RTL837X_PIN_MUX_0, 28);
		reg_bit_set(RTL837X_PIN_MUX_0, 29);
	}
	// LED setup
	// r6520:0021fdb0 R6520-0021e7b0 r6520:0021e7b0 R6520-0021e6b0 r65f8:00000018 R65f8-00000018 R65fc-fffff000 r6600:00000000 R6600-0000000f r65dc:5fffff00 R65dc-7fffff00 r65dc:7fffff00 R65dc-77ffff00
	// r7f8c:30000000 R7f8c-30000000 r7f8c:30000000 R7f8c-38000000 R6548-00410175 r6544:01411000 R6544-01410044 r6528:00000000 R6528-00000011

	// Configure LED_SET_0, ledid 0/1
	REG_SET(RTL837X_REG_LED1_0_SET0, 0x00410175);

	// Configure led-sets 2 and 3
	REG_SET(RTL837X_REG_LED3_2_SET0, 0x01410044);

	// Further configure LED_SET_0
	// r6528:00000000 R6528-00000011
	reg_read_m(RTL837X_REG_LED3_0_SET1);
	sfr_data[3] = 0x11;
	reg_write_m(RTL837X_REG_LED3_0_SET1);
}


void rtl8373_revision(void)
{
	reg_read_m(RTL837X_REG_CHIP_INFO);
	sfr_mask_data(2, 0x0a, 0x0a); 	// Enable reading version
	reg_write_m(RTL837X_REG_CHIP_INFO);
	delay(50);

	reg_read_m(RTL837X_REG_CHIP_INFO);
	print_string("CPU revision: "); print_byte(sfr_data[2]); print_byte(sfr_data[2]); write_char('\n');
	sfr_mask_data(2, 0x0a, 0x00); 	// Enable reading version
	reg_write_m(RTL837X_REG_CHIP_INFO);
}


void rtl8373_init(void)
{
	print_string("\nrtl8373_init called\n");

	led_config_9xh();
	sds_init();
	// Disable all SERDES for configuration
	REG_SET(RTL837X_REG_SDS_MODES, 0x000037ff);

	// q000601:c800 Q000601:c804 q000601:c804 Q000601:c800
	sds_read(0, 0x06, 0x01);
	uint16_t pval = SFR_DATA_U16;
	sds_write_v(0, 0x06, 0x01, pval | 0x04);
	delay(50);
	sds_read(0, 0x06, 0x01);
	pval = SFR_DATA_U16;
	sds_write_v(0, 0x06, 0x01, pval & 0xfffb);

	phy_config_8224();
	sds_config_mac(1, SDS_OFF);    // Off for now until SFP+ port used
	sds_config_mac(2, SDS_SGMII);  // For RTL8224
	sds_config(0, SDS_QXGMII);

	// SDS 1 setup
	// q012100:4902 Q012100:4906 q013605:0000 Q013605:4000 Q011f02:001f q011f15:0086
	sds_write_v(1, 0x21, 0x00, 0x4906);
	sds_write_v(1, 0x36, 0x05, 0x4000);
	sds_write_v(1, 0x1f, 0x02, 0x001f);
	sds_read(1, 0x1f, 0x15);
	pval = SFR_DATA_U16;

	// r0a90:000000f3 R0a90-000000fc
	reg_read_m(0xa90);
	sfr_mask_data(0, 0x0f,0x0c);
	reg_write_m(0xa90);

	rtl8224_phy_enable();

	// Disable PHYs for configuration
	phy_write_mask(0xff,0x1f,0xa610,0x2858);

	// Set bits 0x13 and 0x14 of 0x5fd4
	// r5fd4:0002914a R5fd4-001a914a
	reg_bit_set(0x5fd4, 0x13);
	reg_bit_set(0x5fd4, 0x14);

	// Configure ports
	uint16_t reg = 0x1238; // Port base register for the bits we set
	for (char i = 0; i < 9; i++) {
		// Bit 7 (0x40) enables replacement of the RTL-VLAN tag with an 802.1Q VLAN tag
		REG_SET(reg, 0xe77);
		reg += 0x100;
	}

	// r0b7c:000000d8 R0b7c-000000f8 r6040:00000030 R6040-00000031
	reg_bit_set(0xb7c, 5);

	// R7124-00001050 R7128-00001050 R712c-00001050 R7130-00001050 R7134-00001050 R7138-00001050
	// R713c-00001050 R7140-00001050 R7144-00001050 R7148-00001050
	REG_SET(0x7124, 0x1050); REG_SET(0x7128, 0x1050); REG_SET(0x712c, 0x1050);
	REG_SET(0x7130, 0x1050); REG_SET(0x7134, 0x1050); REG_SET(0x7138, 0x1050);
	REG_SET(0x713c, 0x1050); REG_SET(0x7140, 0x1050); REG_SET(0x7144, 0x1050);
	REG_SET(0x7148, 0x1050);

	reg_bit_set(RTL837X_REG_HW_CONF, 0);

	// TODO: patch the PHYs

	// Re-enable PHY after configuration
	phy_write_mask(0xff,0x1f,0xa610,0x2058);

	// Enables MAC access
	// Set bits 0xc-0x14 of 0x632c to 0x1f8, see rtl8372_init
	// r632c:00000540 R632c-001f8540 // RTL8373: 001ff540
	reg_read_m(0x632c);
	sfr_mask_data(1, 0x70, 0xf0); // The ports of the RTL8824
	sfr_mask_data(2, 0x10, 0x1f);
	reg_write_m(0x632c);

	print_string("\nrtl8373_init done\n");
}


void rtl8372_init(void)
{
	print_string("\nrtl8372_init called\n");

	led_config();

	sds_init();
	phy_config(8);	// PHY configuration: External 8221B?
	phy_config(3);	// PHY configuration: all internal PHYs?
	// Set the MAC SerDes Modes Bits 0-4: SDS 0 = 0x2 (0x2), Bits 5-9: SDS 1: 1f (off)
	// r7b20:00000bff R7b20-00000bff r7b20:00000bff R7b20-00000bff r7b20:00000bff R7b20-000003ff r7b20:000003ff R7b20-000003e2 r7b20:000003e2 R7b20-000003e2
	reg_read_m(RTL837X_REG_SDS_MODES);
	sfr_mask_data(1, 0, 0x03);
	sfr_mask_data(0, 0, 0xe2);
	reg_write_m(RTL837X_REG_SDS_MODES);

	// r0a90:000000f3 R0a90-000000fc
	reg_read_m(0xa90);
	sfr_mask_data(0, 0x0f,0x0c);
	reg_write_m(0xa90);

	// Disable PHYs for configuration
	phy_write_mask(0xf0,0x1f,0xa610,0x2858);

	// Set bits 0x13 and 0x14 of 0x5fd4
	// r5fd4:0002914a R5fd4-001a914a
	reg_bit_set(0x5fd4, 0x13);
	reg_bit_set(0x5fd4, 0x14);

	// Configure ports 3-8:
	//
	// r1538:00000e33 R1538-00000e37 r1538:00000e37 R1538-00000e37 r1538:00000e37 R1538-00000f37
	// [...]
	///
	uint16_t reg = 0x1238 + 0x300; // Port base register for the bits we set
	for (char i = machine.min_port; i <= machine.max_port; i++) {
		// Bit 7 (0x40) enables replacement of the RTL-VLAN tag with an 802.1Q VLAN tag
		REG_SET(reg, 0xe77);
		reg += 0x100;
	}

	// r0b7c:000000d8 R0b7c-000000f8 r6040:00000030 R6040-00000031
	reg_bit_set(0xb7c, 5);

	reg_bit_set(RTL837X_REG_HW_CONF, 0);

	// TODO: patch the PHYs

	// Re-enable PHY after configuration
	phy_write_mask(0xf0,0x1f,0xa610,0x2058);

	// Enables MAC access
	// Set bits 0xc-0x14 of 0x632c to 0x1f8, see rtl8372_init
	// r632c:00000540 R632c-001f8540 // RTL8373: 001ff540
	reg_read_m(0x632c);
	sfr_mask_data(1, 0x70, 0x80);
	sfr_mask_data(2, 0x10, 0x1f);
	reg_write_m(0x632c);
	print_string("\nrtl8372_init done\n");
}


/*
 * The SoC manages Link-State for steering the LEDs and can set PHY-settings
 * automatically through Realtek's SMI (Simple Managagement) Interface, a
 * proprietary version of MDIO which for example allows for more PHYs on the same
 * bus.
 * Configure polling via SMI and the interface setup during boot.
 */
void init_smi(void)
{
	print_string("\ninit_switch called\n");

	/* Set the SMI(i.e.I2C) type for PHY polling, 0b01 is 2.5/10G PHY. Disable (0b00) for the SFP-ports
	 * which are at port 8 and additionally at port 3 for a dual SFP device
	 */
	REG_SET(RTL837X_REG_SMI_MAC_TYPE, machine.n_sfp == 2 ? 0x00005515 : 0x00005555);

	// Configure polling of all PHYs by the MAC to detect link-state changes
	if (machine.isRTL8373) {
		REG_SET(RTL837X_REG_SMI_PORT_POLLING, 0xff);
	} else {
		REG_SET(RTL837X_REG_SMI_PORT_POLLING, machine.n_sfp == 2 ? 0xf0 : 0x1f8);
	}
	// Enable MDC
	reg_read_m(RTL837X_REG_SMI_CTRL);
	sfr_mask_data(1, 0, 0x70); 	// Set bits 12-14 to enable MDC for SMI0-SMI2
	reg_write_m(RTL837X_REG_SMI_CTRL);
	delay(50);

	if (!machine.isRTL8373) {
		// Change I2C addresses for SMI of the non-existent PHYs
		// r6450:000020e6 R6450-000000e6
		reg_read_m(RTL837X_REG_SMI_PORT6_9_ADDR);
		sfr_mask_data(1, 0x7c, 0);
		reg_write_m(RTL837X_REG_SMI_PORT6_9_ADDR);

		// r644c:0a418820 R644c-0a400820
		reg_read_m(RTL837X_REG_SMI_PORT0_5_ADDR);
		sfr_mask_data(2, 0x0f, 0);
		sfr_mask_data(1, 0x80, 0);
		reg_write_m(RTL837X_REG_SMI_PORT0_5_ADDR);
	}
}


/* Set up serial port 0 using Timer 1 as baudrate generator.
 * For x Bd these settings are needed, see table below.
 * NOTE: Settings only valid for F_SYS = 125 MHz!
 * |   Wanted |       | TMR | F_SYS |      |   Actual |        |
 * | baudrate | SMOD0 | DIV |   DIV |  TH1 | baudrate |  Error |
 * | -------- | ----- | --- | ----- | ---- | -------- | ------ |
 * |     1200 |   0   |  12 |   255 | 0x01 |   1276.6 |  6.00% |
 * |     2400 |   0   |  12 |   136 | 0x78 |   2393.5 | −0.27% |
 * |     4800 |   0   |   4 |   203 | 0x35 |   4810.7 |  0.22% |
 * |     9600 |   1   |   4 |   203 | 0x35 |   9621.3 |  0.22% |
 * |    14400 |   1   |   4 |   136 | 0x78 |  14361.2 | −0.27% |
 * |    19200 |   1   |   4 |   102 | 0x9a |  19148.3 | −0.27% |
 * |    38400 |   1   |   4 |    51 | 0xcd |  38296.6 | −0.27% |
 * |    57600 |   1   |   4 |    34 | 0xde |  57444.9 | −0.27% |
 * |   115200 |   1   |   4 |    17 | 0xef | 114889.7 | −0.27% |
 */
#if CLOCK_HZ != 125000000
#warning "SERIAL 0 baudrate setting may only valid for F_CPU = 125 MHz!"
#endif
void setup_serial_timer1(void)
{
	// Timer 1: Mode 2: automatic reload
	TMOD &= 0x0F;
	TMOD |= 0x20; // Timer1: Mode2: Timer, 8-bit with auto-reload
	CKCON |= 0x10; // Timer1 clock divider: F_SYS / 4: T2M = 1, Timer 1 uses clk/4

	PCON |= 0x80; // SMOD0 = 1; Double the Baud Rate, don't divide Timer 1 Overflag signal.

	SCON  = 0x50;  // Mode = 1: ASYNC 8N1 with Timer 2 as baud-rate generator, REN_0 Receive enable

	/* The TH1 register contain the reload value, timer1 when T1 overflows to 0x100.
	 * NOTE: compiler computs the wrong value. 0xF0 is calculated but 0xEF is the right value for 115200.
	 * Also https://www.keil.com/products/c51/baudrate.asp confirms this.
	 * Added 32 before div by 64 to make sure rounding is correct so that the results are right.
	 *
	 * TH1 = 0x100 - (2^SMOD0 * F_SYS) / ( TMR1_DIV / BAUDRATE * 32)
	 */
	TH1 = (0x100 - (((CLOCK_HZ / SERIAL_BAUD_RATE) + 32) / (4 * 16))) & 0xff;

	TCON |= 0x40;	// Start timer 1

	ET1 = 0; // Timer1 Interrupt is NOT wanted!
	TI = 0; // Clear TI-interrupt flag
	RI = 0; // Clear RI-interrupt flag

	tx_buf_empty = 1; // Set tx `serial buffer is empty`-software flag.

	ES = 1; // Enable serial IRQ
}


void setup_i2c(void)
{
	REG_SET(RTL837X_REG_I2C_MST_IF_CTRL, 0);
	// Configure SFP EEPROM address (0x50) as I2C device address
	// Configure SFP readings address (0x51) as I2C device address
	REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00, 0x1 << (I2C_MEM_ADDR_WIDTH-16),  0x50 >> 5, (0x50 << 3) & 0xff);

	REG_SET(RTL837X_REG_I2C_CTRL2, 0);

	// HW Control register, enable I2C?
	reg_read_m(RTL837X_PIN_MUX_1);
	sfr_mask_data(3, 0x20, 0x00); // Clear bit 29
	sfr_mask_data(0, 0x60, 0x40); // Set bits 5-6 to 0b10
	reg_write_m(RTL837X_PIN_MUX_1);
}


void bootloader(void)
{
	ticks = 0;
	stp_clock = STP_TICK_DIVIDER;
	dhcp_state.state = DHCP_OFF;
	sbuf_ptr = 0;

	CKCON = 0;	// Initial Clock configuration
	SFR_97 = 0;	// HADDR?

	// Set in managed mode:
	SFR_b9 = 0x00;
	SFR_ba = 0x80;

	// Disable all interrupts (global and individually) by setting IE register (SFR A8) to 0
	IE = 0;
	EIE = 0;  // SFR e8: EIE. Disable all external IRQs

	idle_ready = 0;
	// HW setup, serial, timer, external IRQs
	setup_clock();
	setup_timer2();
	setup_serial_timer1();
	setup_external_irqs();

	EA = 1; // Enable global interrupt

	// Flash controller should be initialized before any code in other banks is being fetched
	// See this issue: https://github.com/logicog/RTLPlayground/issues/70
	print_string("\nInitializing Flash controller\n");
	flash_init(1);

	// Set default for SFP pins so we can start up a module already inserted
	sfp_pins_last = 0x33; // signal LOS and no module inserted (for both slots, even if only 1 present)
	// We have not detected any link
	linkbits_last[0] = linkbits_last[1] = linkbits_last[2] = linkbits_last[3] = linkbits_last_p89 = 0;

	print_string("Detecting CPU: ");
	reg_read_m(0x4);
	if (sfr_data[1] == 0x73) { // Register was 0x83730000
		print_string("RTL8373\n");
		if (!machine.isRTL8373)
			print_string("INCORRECT MACHINE!");
		rtl8224_enable();  // Power on the RTL8224
	} else {
		print_string("RTL8372\n");
		if (machine.isRTL8373)
			print_string("INCORRECT MACHINE!");
	}

	// Print SW version
	print_sw_version();

	// Reset NIC
	reg_bit_set(RTL837X_REG_RESET, RESET_NIC_BIT);
	do {
		reg_read(RTL837X_REG_RESET);
	} while (SFR_DATA_0 & (1 << RESET_NIC_BIT));
	print_string("NIC reset\n");

	uip_ipaddr(&uip_hostaddr, ownIP[0], ownIP[1], ownIP[2], ownIP[3]);
	uip_ipaddr(&uip_draddr, gatewayIP[0], gatewayIP[1], gatewayIP[2], gatewayIP[3]);
	uip_ipaddr(&uip_netmask, netmask[0], netmask[1], netmask[2], netmask[3]);

	REG_SET(RTL837X_PIN_MUX_2, 0x0); // Disable pins for ACL
	init_smi();
	rtl8373_revision();
	if (machine.isRTL8373)
		rtl8373_init();
	else
		rtl8372_init();
	delay(1000);

	// Check update in progress and move blocks
	flash_region.addr = FIRMWARE_UPLOAD_START;
	flash_region.len = 0x100;
	flash_read_bulk(flash_buf);

	if (flash_buf[0] == 0x00 && flash_buf[1] == 0x40) {
		__xdata uint32_t dest = 0x0;
		__xdata uint32_t source = FIRMWARE_UPLOAD_START;
		__xdata uint16_t i = 0;
		__xdata uint16_t j = 0;
		__xdata uint8_t * __xdata bptr;
		print_string("Identified update image. Checking integrity...");

		flash_init(0); // Re-initialize flash for non-DIO operation, otherwise flashing will fail
		set_sys_led_state(SYS_LED_FAST);

		crc_value = 0x0000;
		for (i = 0; i < 1024; i++) {
			flash_region.addr = source;
			flash_region.len = 0x200;
			flash_read_bulk(flash_buf);
			bptr = flash_buf;
			for (j = 0; j < 0x200; j++) {
			//	print_byte(*bptr); write_char(' ');
				crc16(bptr++);
			//	print_short(crc_value); write_char(':');
			}
			source += 0x200;
			// write_char('\n'); print_short(crc_value); write_char(' ');
			if (i%16 == 0)
				write_char('.');
		}
		if (crc_value == 0xb001) {
			print_string("\nChecksum OK\n");
			print_string("Update in progress, moving firmware to start of FLASH.");
			source = FIRMWARE_UPLOAD_START;
			// A 512kByte = 4MBit Flash has 128*8=1024 512byte blocks, we copy only 896 
			// (don't overwrite config @ 0x700000)
			for (i = 0; i < 896; i++) {
				// print_string("Writing block: ");
				// print_short(dest);
				flash_region.addr = source;
				flash_region.len = 0x200;
				flash_read_bulk(flash_buf);
				if (!(i & 0x7)) {
					flash_region.addr = dest;
					flash_sector_erase();
					write_char('.');
				}
				flash_region.addr = dest;
				flash_region.len = 0x200;
				flash_write_bytes(flash_buf);
				dest += 0x200;
				source += 0x200;
			}
			print_string("\nDeleting uploaded flash image\n");
			dest = FIRMWARE_UPLOAD_START;
			for (register uint8_t i=0; i < 128; i++) {
				flash_region.addr = dest;			
				flash_sector_erase();
				dest += 0x1000;
			}
			print_string("Resetting now");
			delay(200);
			reset_chip();
		}
		print_string("Checksum incorrect, please upload the image again\n");
		print_string("Erasing bad uploaded flash image\n");
		dest = FIRMWARE_UPLOAD_START;
		for (register uint8_t i=0; i < 128; i++) {
			flash_region.addr = dest;
			flash_sector_erase();
			dest += 0x1000;
		}
	}

	set_sys_led_state(SYS_LED_SLOW);

#ifdef DEBUG
	// This register seems to work on the RTL8373 only if also the SDS
	// Is correctly configured. Therefore, we can test it, here...
	// Reset seconds counter
	print_string("\nTIMER-TEST: \n");
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
	stpEnabled = 0;
	nic_setup();
	vlan_setup();
	port_l2_setup();
	igmp_setup();
	uip_init();
	uip_arp_init();
	httpd_init();

	management_vlan = 0; // Disabled

	setup_i2c();

	print_string(greeting);

	print_string("\nClock register: ");
	print_reg(0x6040);
	print_string("\nRegister 0x7b20/RTL837X_REG_SDS_MODES: ");
	print_reg(0x7b20);

	print_string("\nVerifying PHY settings:\n");
//	p031f.a610:2058 p041f.a610:2058  p051f.a610:2058  r4f3c:00000000 p061f.a610:2058 p071f.a610:2058 
	port_stats_print();

	execute_config();
	print_string("\n> ");
	idle_ready = 1;

	set_sys_led_state(SYS_LED_ON);

	// Wait for commands on serial connection
	// sbuf_ptr is moved forward by serial interrupt, l is the position until we have already
	// printed out the entered characters
	__xdata uint8_t l = sbuf_ptr; // We have printed out entered characters until l
	__xdata uint8_t line_start = sbuf_ptr; // This is where the current line starts
	cmd_available = 0;
	while (1) {
		while (l != sbuf_ptr) {
			// If the command buffer is currently in use, we cannot copy to it
			if (cmd_available)
				break;
			write_char(sbuf[l]);
			// Check whether there is a full line:
			if (sbuf[l] == '\n' || sbuf[l] == '\r') {
				write_char('\n');
				register uint8_t i = 0;
				while (line_start != l) {
					cmd_buffer[i++] = sbuf[line_start++];
					line_start &= (SBUF_SIZE - 1);
				}
				line_start++;
				line_start &= (SBUF_SIZE - 1);
				cmd_buffer[i] = '\0';
				// If there is a command we print the prompt after execution
				// otherwise immediately because there is nothing to execute
				if (i)
					cmd_available = 1;
				else
					print_string("\n> ");
			}
			l++;
			l &= (SBUF_SIZE - 1);
		}
		idle(); // Enter Idle mode until interrupt occurs
	}
}

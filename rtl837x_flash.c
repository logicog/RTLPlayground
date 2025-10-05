/*
 * This is driver implementation for the RTL837x flash controller
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"

__xdata uint8_t dio_enabled;
__xdata uint8_t markbuf[16];
extern __xdata uint16_t mpos;
__xdata struct flash_region_t flash_region;


// For the flash commands, see e.g. Windbond W25Q32JV datasheet
#define CMD_WRITE_STATUS	0x01
#define CMD_PAGE_PROGRAM	0x02
#define CMD_READ		0x03
#define CMD_WRITE_ENABLE	0x06
#define CMD_FREAD		0x0b
#define CMD_SECTOR_ERASE	0x20
#define CMD_READ_SECURITY_REGS	0x48
#define CMD_READ_UNIQUE_ID	0x4b
#define CMD_READ_JEDEC_ID	0x9f
#define CMD_FREAD_DIO		0xbb

/*
 * Configure Memory Managed IO
 */
void flash_configure_mmio(void)
{
	// Set configuration for MMIO access by controller
	if (dio_enabled) {
		SFR_FLASH_MODEB = 0x18;
		SFR_FLASH_CMD_R = CMD_FREAD_DIO;	// By default we read with Dual speed
		SFR_FLASH_DUMMYCYCLES = 4;
		return;
	}

	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = CMD_FREAD; // Default is Single IO
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
		SFR_FLASH_CONFIG = 9;  // There may be a chip-select in here
		SFR_FLASH_CONF_RCMD = CMD_FREAD_DIO;
		SFR_FLASH_CONF_DIV = 4;
	} else {
		// Configure fast read via divider = 8 and read-cmd being CMD_FREAD (for mmio)
		SFR_FLASH_CONFIG = 9;
		SFR_FLASH_CONF_RCMD = CMD_FREAD;
		SFR_FLASH_CONF_DIV = 8;
	}
	// Test Controller Busy
	while(SFR_FLASH_EXEC_BUSY);

	// Write 0 to status register
	SFR_FLASH_DUMMYCYCLES = 8;
	SFR_FLASH_MODEB = 0;
	SFR_FLASH_TCONF = 0x19;
	SFR_FLASH_CMD = CMD_WRITE_STATUS;
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


void flash_read_uid(void)
{
	while (flash_read_status() & 0x1);

	// Set slow read mode for UID
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = CMD_READ_UNIQUE_ID;
	SFR_FLASH_DUMMYCYCLES = 8;

	// Transfer 4 bytes (command + 3 dummy bytes)
	SFR_FLASH_TCONF = 4;
	SFR_FLASH_ADDR16 = 0;
	SFR_FLASH_ADDR8 = 0;
	SFR_FLASH_ADDR0 = 0;

	SFR_FLASH_EXEC_GO = 1;
	while(SFR_FLASH_EXEC_BUSY);

	print_byte(SFR_FLASH_DATA0);
	print_byte(SFR_FLASH_DATA8);
	print_byte(SFR_FLASH_DATA16);
	print_byte(SFR_FLASH_DATA24);

	SFR_FLASH_EXEC_GO = 1;
	SFR_FLASH_DUMMYCYCLES = 24;
	while(SFR_FLASH_EXEC_BUSY);

	print_byte(SFR_FLASH_DATA0);
	print_byte(SFR_FLASH_DATA8);
	print_byte(SFR_FLASH_DATA16);
	print_byte(SFR_FLASH_DATA24);

	flash_configure_mmio();
}


void flash_read_jedecid(void)
{
	while (flash_read_status() & 0x1);

	// Set read mode for JEDEC ID
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = CMD_READ_JEDEC_ID;
	SFR_FLASH_DUMMYCYCLES = 0;

	// Transfer 3 bytes back
	SFR_FLASH_TCONF = 0x13;

	SFR_FLASH_EXEC_GO = 1;
	while(SFR_FLASH_EXEC_BUSY);

	print_byte(SFR_FLASH_DATA0);
	print_byte(SFR_FLASH_DATA8);
	print_byte(SFR_FLASH_DATA16);
	print_byte(SFR_FLASH_DATA24);

	// Reset slow read mode
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = CMD_FREAD;
	SFR_FLASH_DUMMYCYCLES = 8;
}


void flash_write_enable(void)
{
	short status;

	// Wait until busy bit clear
	do {
		status = flash_read_status();
	} while (status & 0x1);

	SFR_FLASH_TCONF = 0x18;
	SFR_FLASH_CMD = CMD_WRITE_ENABLE;

	/* The following makes sure that the PAGE_PROGRAM command,
	 * where the data to be written follows the command word directly
	 * works properly
	 */
	SFR_FLASH_DUMMYCYCLES = 0;
	SFR_FLASH_MODEB = 0;

	SFR_FLASH_EXEC_GO = 1;
	// Wait for write status enabled
	do {
		status = flash_read_status();
	} while (!(status & 0x2));
}


void flash_dump(uint8_t len)
{
	short status;
	do {
		status = flash_read_status();
		print_short(status);
	} while (status & 0x1);

	// Set fast read mode
	if (dio_enabled) {
		SFR_FLASH_MODEB = 0x18;
		SFR_FLASH_CMD_R = CMD_FREAD_DIO;
		SFR_FLASH_DUMMYCYCLES = 4;
	} else {
		SFR_FLASH_MODEB = 0x0;
		SFR_FLASH_CMD_R = CMD_FREAD;	// Fast read
		SFR_FLASH_DUMMYCYCLES = 8;	// Add 8 dummy clocks after read?
	}
	// Read 4 bytes
	SFR_FLASH_TCONF = 4;
	while (len) {
		SFR_FLASH_ADDR16 = flash_region.addr >> 16;
		SFR_FLASH_ADDR8 = flash_region.addr >> 8;
		SFR_FLASH_ADDR0 = flash_region.addr;
		flash_region.addr += 4;

		SFR_FLASH_EXEC_GO = 1;
		while(SFR_FLASH_EXEC_BUSY);

		print_short(SFR_FLASH_DATA0);
		if (len == 1)
			return;
		print_short(SFR_FLASH_DATA8);
		if (len == 2)
			return;
		print_short(SFR_FLASH_DATA16);
		if (len == 3)
			return;
		print_short(SFR_FLASH_DATA24);

		len -= 4;
	}
}

/*
 * Reads bulk data of length len from the flash memory starging at address src
 * and writes the data into a buffer pointed to by dst in XMEM
 */
void flash_read_bulk(__xdata uint8_t *dst)
{
	short status;
	do {
		status = flash_read_status();
	} while (status & 0x1);

	// Set fast read mode
	if (dio_enabled) {
		SFR_FLASH_MODEB = 0x18;
		SFR_FLASH_CMD_R = CMD_FREAD_DIO;
		SFR_FLASH_DUMMYCYCLES = 4;
	} else {
		SFR_FLASH_MODEB = 0x0;
		SFR_FLASH_CMD_R = CMD_READ;
		SFR_FLASH_DUMMYCYCLES = 0;
	}


	// Read 4 bytes
	while (1) {
		SFR_FLASH_ADDR16 = flash_region.addr >> 16;
		SFR_FLASH_ADDR8 = flash_region.addr >> 8;
		SFR_FLASH_ADDR0 = flash_region.addr;
		flash_region.addr += 4;

		SFR_FLASH_TCONF = 4;

		SFR_FLASH_EXEC_GO = 1;
		while(SFR_FLASH_EXEC_BUSY);

		*dst++ = SFR_FLASH_DATA0;
		if (flash_region.len == 1)
			break;
		*dst++ = SFR_FLASH_DATA8;
		if (flash_region.len == 2)
			break;
		*dst++ = SFR_FLASH_DATA16;
		if (flash_region.len == 3)
			break;
		*dst++ = SFR_FLASH_DATA24;
		if (flash_region.len == 4)
			break;
		flash_region.len -= 4;
	}
}


void flash_read_security()
{
	while (flash_read_status() & 0x1);

	// Set slow read mode
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = CMD_READ_SECURITY_REGS;		// read security register
	SFR_FLASH_DUMMYCYCLES = 8;	// Add 8 dummy clocks as for fast read

	// Transfer 4 bytes (command + 3byte address)
	SFR_FLASH_TCONF = 4;
	do {
		SFR_FLASH_ADDR16 = flash_region.addr >> 16;
		SFR_FLASH_ADDR8 = flash_region.addr >> 8;
		SFR_FLASH_ADDR0 = flash_region.addr;
		flash_region.addr += 4;

		SFR_FLASH_EXEC_GO = 1;
		while(SFR_FLASH_EXEC_BUSY);

		print_byte(SFR_FLASH_DATA0);
		if (flash_region.len == 1)
			break;
		print_byte(SFR_FLASH_DATA8);
		if (flash_region.len == 2)
			break;
		print_byte(SFR_FLASH_DATA16);
		if (flash_region.len == 3)
			break;
		print_byte(SFR_FLASH_DATA24);

		flash_region.len -= 4;
	} while(flash_region.len);
}


void flash_sector_erase(void)
{
	flash_write_enable();
	SFR_FLASH_TCONF = 8;
	SFR_FLASH_CMD = CMD_SECTOR_ERASE;

	SFR_FLASH_ADDR16 = flash_region.addr >> 16;
	SFR_FLASH_ADDR8 = flash_region.addr >> 8;
	SFR_FLASH_ADDR0 = flash_region.addr;

	SFR_FLASH_EXEC_GO = 1;
	while (flash_read_status() & 0x1);

	flash_configure_mmio();
}


void flash_write_bytes(__xdata uint8_t *ptr)
{
	write_char('>'); print_long(flash_region.addr); write_char(':'); print_short(flash_region.len); write_char('-'); print_byte(*ptr); write_char('\n');
		while(1) {
		flash_write_enable();
		SFR_FLASH_CMD = CMD_PAGE_PROGRAM;
		SFR_FLASH_TCONF = 0x40 | 8 | 4; // Bytes written is 4, 8 enables write, 0x40 is unknown
		// Last transfer?
		if (flash_region.len < 5) {
			SFR_FLASH_TCONF = 8 | flash_region.len;
		}

		SFR_FLASH_ADDR16 = flash_region.addr >> 16;
		SFR_FLASH_ADDR8 = flash_region.addr >> 8;
		SFR_FLASH_ADDR0 = flash_region.addr;
		SFR_FLASH_DATA0 = *ptr++;
		SFR_FLASH_DATA8 = *ptr++;
		SFR_FLASH_DATA16 = *ptr++;
		SFR_FLASH_DATA24 = *ptr++;

		// Execute transfer, we wait for completion at top of loop
		SFR_FLASH_EXEC_GO = 1;

		if (flash_region.len < 5)
			break;

		flash_region.len -= 4;
		flash_region.addr += 4;
	};

	while (flash_read_status() & 0x1);
	flash_configure_mmio();
}

/*
 * This is driver implementation for the RTL837x flash controller
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_stdio.h"
#include "rtl837x_sfr.h"

__xdata uint8_t dio_enabled;


/*
 * Configure Memory Managed IO
 */
void flash_configure_mmio(void)
{
	// Set configuration for MMIO access by controller
	if (dio_enabled) {
		SFR_FLASH_MODEB = 0x18;
		SFR_FLASH_CMD_R = 0xbb;	// By default we read with Dual speed
		SFR_FLASH_DUMMYCICLES = 4;
		return;
	}

	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0xb;	// By default we read with single speed
	SFR_FLASH_DUMMYCICLES = 8;
}


/*
 * Initializes the flash controller for programmed control
 * The configuration options are not really understood, the SPI speed
 * seems to be directly linked to the CPU frequency
 * This configures uses fast single IO at 20.8 MHz when the CPU clock is at 20.8MHz
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
	SFR_FLASH_DUMMYCICLES = 8;
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


void flash_read_uid(void)
{
	while (flash_read_status() & 0x1);

	// Set slow read mode for UID
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0x4b;
	SFR_FLASH_DUMMYCICLES = 8;

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
	SFR_FLASH_DUMMYCICLES = 24;
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
	SFR_FLASH_CMD_R = 0x9f;
	SFR_FLASH_DUMMYCICLES = 0;

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
	SFR_FLASH_CMD_R = 0xb;
	SFR_FLASH_DUMMYCICLES = 8;
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
/*	The following is explicitly set for SIO, is this necessary?:
	SFR_FLASH_DUMMYCYCLES = 0;
	SFR_FLASH_MODEB = 0;
*/

	SFR_FLASH_EXEC_GO = 1;
	// Wait for write status enabled
	do {
		status = flash_read_status();
	} while (!(status & 0x2));
}


void flash_dump(uint32_t addr, uint8_t len)
{
	short status;
	do {
		status = flash_read_status();
		print_short(status);
	} while (status & 0x1);

	// Set fast read mode
	if (dio_enabled) {
		SFR_FLASH_MODEB = 0x18;
		SFR_FLASH_CMD_R = 0xbb;
		SFR_FLASH_DUMMYCICLES = 4;
	} else {
		SFR_FLASH_MODEB = 0x0;
		SFR_FLASH_CMD_R = 0xb;	// Fast read
		SFR_FLASH_DUMMYCICLES = 8;	// Add 8 dummy clocks after read?
	}
	// Read 4 bytes
	SFR_FLASH_TCONF = 4;
	while (len) {
		SFR_FLASH_ADDR16 = addr >> 16;
		SFR_FLASH_ADDR8 = addr >> 8;
		SFR_FLASH_ADDR0 = addr;
		addr += 4;

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


void flash_read_security(uint32_t addr, uint8_t len)
{
	while (flash_read_status() & 0x1);

	// Set slow read mode
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0x48;		// read security register
	SFR_FLASH_DUMMYCICLES = 8;	// Add 8 dummy clocks as for fast read

	// Transfer 4 bytes (command + 3byte address)
	SFR_FLASH_TCONF = 4;
	while (len) {
		SFR_FLASH_ADDR16 = addr >> 16;
		SFR_FLASH_ADDR8 = addr >> 8;
		SFR_FLASH_ADDR0 = addr;
		addr += 4;

		SFR_FLASH_EXEC_GO = 1;
		while(SFR_FLASH_EXEC_BUSY);

		print_byte(SFR_FLASH_DATA0);
		if (len == 1)
			return;
		print_byte(SFR_FLASH_DATA8);
		if (len == 2)
			return;
		print_byte(SFR_FLASH_DATA16);
		if (len == 3)
			return;
		print_byte(SFR_FLASH_DATA24);

		len -= 4;
	}
}


void flash_block_erase(uint32_t addr)
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


void flash_write_bytes(uint32_t addr, __xdata uint8_t *ptr, uint8_t len)
{
	uint8_t exit_loop = 0;

	while(1) {
		flash_write_enable();
		SFR_FLASH_CMD = 2;
		SFR_FLASH_TCONF = 0x40 | 8 | 2; // Bytes written is is 4, 8 enables write, 0x2 is unkown
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

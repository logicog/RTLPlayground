/*
 * This is driver implementation for the RTL837x flash controller
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_stdio.h"
#include "rtl837x_sfr.h"


// DE62B415D3155829
// JEDEC-ID: ef 40 16

/*
void flash_init()
{
	// Configure fast read via divider = 4 and read-cmd being 0xbb (for mmio)
	SFR_FLASH_CONFIG = 9;
	SFR_FLASH_CONF_RCMD = 0xbb;
	SFR_FLASH_CONF_DIV = 4;

	// Test Controller Busy
	while(SFR_FLASH_EXEC_BUSY);

	// Write 0 to status register
	SFR_FLASH_DUMMYCICLES = 0;
	SFR_FLASH_MODEB = 0;
	SFR_FLASH_TLEN = 0x19;
	SFR_FLASH_CMD = 1;
	SFR_FLASH_DATA0 = 0;
	SFR_FLASH_EXEC_GO = 1;
	while(SFR_FLASH_EXEC_BUSY);

	// Set some idle mode
	SFR_FLASH_MODEB = 0x18;
	SFR_FLASH_CMD_R = 0xbb;	// By default we read with Dual speed
	SFR_FLASH_DUMMYCICLES = 4;
}
*/

void flash_init(void)
{
	// Configure fast read via divider = 8 and read-cmd being 0xb (for mmio)
	SFR_FLASH_CONFIG = 9;
	SFR_FLASH_CONF_RCMD = 0xb;
	SFR_FLASH_CONF_DIV = 8;

	// Test Controller Busy
	while(SFR_FLASH_EXEC_BUSY);

	// Write 0 to status register
	SFR_FLASH_DUMMYCICLES = 8;
	SFR_FLASH_MODEB = 0;
	SFR_FLASH_TLEN = 0x19;
	SFR_FLASH_CMD = 1;
	SFR_FLASH_DATA0 = 0;
	SFR_FLASH_EXEC_GO = 1;
	while(SFR_FLASH_EXEC_BUSY);

	// Set some idle mode
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0xb;	// By default we read with single speed
	SFR_FLASH_DUMMYCICLES = 8;
}

uint8_t flash_read_status(void)
{
	// Test Controller Busy (we might call this directly after executing a command)
	while(SFR_FLASH_EXEC_BUSY);

	// setup status read command
	SFR_FLASH_TLEN = 0x11;
	SFR_FLASH_CMD_R = 5;

	// execute and wait for controller done
	SFR_FLASH_EXEC_GO = 1;
	while(SFR_FLASH_EXEC_BUSY);

	return SFR_FLASH_DATA0;
}


void flash_read_uid(void)
{
	while (flash_read_status() & 0x1);
	print_string("\r\n -uid- \r\n");

	// Set slow read mode for UID
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0x4b;
	SFR_FLASH_DUMMYCICLES = 8;

	// Transfer 4 bytes (command + 3 dummy bytes)
	SFR_FLASH_TLEN = 4;
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

	// Reset slow read mode
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0xb;
	SFR_FLASH_DUMMYCICLES = 8;
}


void flash_read_jedecid(void)
{
	while (flash_read_status() & 0x1);
	print_string("\r\n -jedec- \r\n");

  /*
	// Set slow read mode for UID
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0x9f;
	SFR_FLASH_DUMMYCICLES = 8;

	// Transfer 3 bytes back
	SFR_FLASH_TLEN = 0x3;

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

	while (flash_read_status() & 0x1);
	print_string("\r\n -jedec- b- \r\n");

	// Set slow read mode for UID
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0x9f;
	SFR_FLASH_DUMMYCICLES = 8;

	// Transfer 3 bytes back
	SFR_FLASH_TLEN = 0x13;

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

	while (flash_read_status() & 0x1);
	print_string("\r\n -jedec- c \r\n");
*/
	// Set read mode for JEDEC ID
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0x9f;
	SFR_FLASH_DUMMYCICLES = 0;

	// Transfer 3 bytes back
	SFR_FLASH_TLEN = 0x13;

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
		print_short(status);
	} while (status & 0x1);
// 	while (flash_read_status() & 0x1);

	print_string("\r\n -setting- \r\n");
	SFR_FLASH_TLEN = 0x18;
	SFR_FLASH_CMD = 6;

	SFR_FLASH_EXEC_GO = 1;
	do {
		status = flash_read_status();
		print_short(status);
	} while (!(status & 0x2));
}

/*
void flash_dump(uint32_t addr, uint8_t len)
{
	short status;
	print_string("\r\n -a- \r\n");
	do {
		status = flash_read_status();
		print_short(status);
	} while (status & 0x1);
//	while (flash_read_status() & 0x1);
	print_string("\r\n -b- \r\n");

	// Set fast read mode
	SFR_FLASH_MODEB = 0x18;
	SFR_FLASH_CMD_R = 0xbb;
	SFR_FLASH_DUMMYCICLES = 4;

	// Read 4 bytes
	SFR_FLASH_TLEN = 4;
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
*/


void flash_read_security(uint32_t addr, uint8_t len)
{
	while (flash_read_status() & 0x1);
	print_string("\r\n -b- \r\n");

	// Set slow read mode
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0x48;		// read security register
	SFR_FLASH_DUMMYCICLES = 8;	// Add 8 dummy clocks as for fast read

	// Transfer 4 bytes (command + 3byte address)
	SFR_FLASH_TLEN = 4;
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


void flash_dump(uint32_t addr, uint8_t len)
{
	while (flash_read_status() & 0x1);
	print_string("\r\n -b- \r\n");

	// Set slow read mode
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_CMD_R = 0xb;	// Fast read
	SFR_FLASH_DUMMYCICLES = 8;	// Add 8 dummy clocks after read?

	// Transfer 4 bytes (command + 3byte address)
	SFR_FLASH_TLEN = 4;
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

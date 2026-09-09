/*
 * This is driver implementation for the RTL837x flash controller
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"

__xdata uint8_t dio_enabled;
__xdata struct flash_region_t flash_region;

__xdata uint32_t flash_size;
__xdata uint8_t flash_capacity_code;

// For the flash commands, see e.g. Windbond W25Q32JV datasheet
#define CMD_WRITE_STATUS	0x01
#define CMD_PAGE_PROGRAM	0x02
// Don't use command `READ 0x03`, because on many device this command can't run at maximum SPI-clock speed.
// Use `Fast READ 0x0b` instead!
//#define CMD_READ		0x03
#define CMD_READ_STATUS		0x05
#define CMD_WRITE_ENABLE	0x06
#define CMD_FREAD			0x0b
#define CMD_SECTOR_ERASE	0x20
#define CMD_READ_SECURITY_REGS	0x48
#define CMD_READ_UNIQUE_ID	0x4b
#define CMD_READ_JEDEC_ID	0x9f
#define CMD_FREAD_DIO		0xbb

#define STATUS_REG_BUSY_MASK	0x01
#define STATUS_REG_WEL_MASK		0x02

/*
 * Configure Memory Managed IO
 */
static void flash_configure_mmio()
{
	while(SFR_FLASH_EXEC_BUSY);

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

static void flash_configure_sio()
{
	while(SFR_FLASH_EXEC_BUSY);

	// Set configuration for SIO access by controller
	SFR_FLASH_MODEB = 0x0;
	SFR_FLASH_DUMMYCYCLES = 0;
}

static uint8_t flash_read_status(void)
{
    uint8_t status;
    uint8_t old_cmd_r;
    uint8_t old_tconf;

    while(SFR_FLASH_EXEC_BUSY);

    // SAVE SPI peripheral state at entry
    old_cmd_r = SFR_FLASH_CMD_R;
    old_tconf = SFR_FLASH_TCONF;

    SFR_FLASH_TCONF = 0x11;
    SFR_FLASH_CMD_R = CMD_READ_STATUS; 
    
    SFR_FLASH_EXEC_GO = 1;
    while(SFR_FLASH_EXEC_BUSY);
    status = SFR_FLASH_DATA0;

    // RESTORE SPI peripheral state
    SFR_FLASH_CMD_R = old_cmd_r;
    SFR_FLASH_TCONF = old_tconf;

    return status;
}

static void flash_write_enable(void)
{
	short status;

	while (flash_read_status() & STATUS_REG_BUSY_MASK);
	
	while (!(status & STATUS_REG_WEL_MASK)) {	
		while(SFR_FLASH_EXEC_BUSY);

		SFR_FLASH_TCONF = 0x18;
		SFR_FLASH_CMD = CMD_WRITE_ENABLE;
		
		SFR_FLASH_EXEC_GO = 1;
		status = flash_read_status();
	}
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

void flash_read_uid(void)
{
	flash_configure_sio();
	while (flash_read_status() & STATUS_REG_BUSY_MASK);

	// Set slow read mode for UID
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
	write_char(' ');

	SFR_FLASH_DUMMYCYCLES = 24;	// Doesn't seem to work; we get the same data as for the first transfer
	SFR_FLASH_EXEC_GO = 1;
	while(SFR_FLASH_EXEC_BUSY);

	print_byte(SFR_FLASH_DATA0);
	print_byte(SFR_FLASH_DATA8);
	print_byte(SFR_FLASH_DATA16);
	print_byte(SFR_FLASH_DATA24);

	flash_configure_mmio();
}

__code char* get_flash_size_str(void)
{
	switch (flash_capacity_code) {
		case 0x12: return "256 KB";
		case 0x13: return "512 KB";
		case 0x14: return "1 MB";
		case 0x15: return "2 MB";
		case 0x16: return "4 MB";
		case 0x17: return "8 MB";
		case 0x18: return "16 MB";
		default: return "unknown";
	}
}

void flash_read_jedecid(void)
{
	flash_configure_sio();

	while (flash_read_status() & STATUS_REG_BUSY_MASK);

	// Set read mode for JEDEC ID
	SFR_FLASH_CMD_R = CMD_READ_JEDEC_ID;

	// Transfer 3 bytes back
	SFR_FLASH_TCONF = 0x13;

	SFR_FLASH_EXEC_GO = 1;
	while(SFR_FLASH_EXEC_BUSY);

	print_string("Flash information:\n");
	print_string("  Manufacturer ID: 0x");
	print_byte(SFR_FLASH_DATA0);
	print_string("\n  Memory Type:     0x");
	print_byte(SFR_FLASH_DATA8);
	print_string("\n  Capacity:        0x");
	flash_capacity_code = SFR_FLASH_DATA16;
	flash_size = 1UL << flash_capacity_code;
	print_byte(flash_capacity_code);
	print_string(" = "); print_string(get_flash_size_str()); write_char('\n');

	flash_configure_mmio();
}


/*
 * Reads bulk data of length len from the flash memory starging at address src
 * and writes the data into a buffer pointed to by dst in XMEM
 */
void flash_read_bulk(__xdata uint8_t *dst)
{
	short status;
	flash_configure_sio();
	while (flash_read_status() & STATUS_REG_BUSY_MASK);
	flash_configure_mmio();


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


void flash_read_security(void)
{
	flash_configure_sio();

	while (flash_read_status() & STATUS_REG_BUSY_MASK);

	// Set slow read mode
	SFR_FLASH_CMD_R = CMD_READ_SECURITY_REGS;		// read security register

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
		write_char(' ');
		flash_region.len -= 4;
	} while(flash_region.len);

	flash_configure_mmio();
}


void flash_sector_erase(void)
{
	flash_configure_sio();
	flash_write_enable();
	SFR_FLASH_TCONF = 8;
	SFR_FLASH_CMD = CMD_SECTOR_ERASE;

	SFR_FLASH_ADDR16 = flash_region.addr >> 16;
	SFR_FLASH_ADDR8 = flash_region.addr >> 8;
	SFR_FLASH_ADDR0 = flash_region.addr;

	SFR_FLASH_EXEC_GO = 1;
	while (flash_read_status() & STATUS_REG_BUSY_MASK);

	flash_configure_mmio();
}


void flash_write_bytes(__xdata uint8_t *ptr)
{
    flash_configure_sio();
    
    while(1) {
        flash_write_enable();
        SFR_FLASH_CMD = CMD_PAGE_PROGRAM;
        
		// Last transfer?
        if (flash_region.len < 5) {
            SFR_FLASH_TCONF = 8 | flash_region.len;
        } else {
            SFR_FLASH_TCONF = 0x40 | 8 | 4;  // Bytes written is 4, 8 enables write, 0x40 is unknown
        }

        SFR_FLASH_ADDR16 = flash_region.addr >> 16;
        SFR_FLASH_ADDR8 = flash_region.addr >> 8;
        SFR_FLASH_ADDR0 = flash_region.addr;
        
        // Safely load only the valid bytes remaining in the buffer
        SFR_FLASH_DATA0 = *ptr++;
        if (flash_region.len > 1) SFR_FLASH_DATA8 = *ptr++;
        if (flash_region.len > 2) SFR_FLASH_DATA16 = *ptr++;
        if (flash_region.len > 3) SFR_FLASH_DATA24 = *ptr++;

        // Execute transfer, wait for completion at top of loop
        SFR_FLASH_EXEC_GO = 1;

        if (flash_region.len < 5)
            break;

        flash_region.len -= 4;
        flash_region.addr += 4;
    };
    
    while (flash_read_status() & STATUS_REG_BUSY_MASK);
    flash_configure_mmio();
}

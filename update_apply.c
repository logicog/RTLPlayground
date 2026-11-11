/*
 * update_apply.c - apply a staged firmware image at boot. See update_apply.h.
 *
 * Moved verbatim out of rtlplayground.c so it can be exercised on the host test
 * bench (test/test_update_apply.c) against a modelled flash device, without a
 * programmer or a switch. No behaviour change.
 */
#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_flash.h"
#include "rtl837x_regs.h"
#include "update_apply.h"

extern __xdata uint32_t flash_size;
extern __xdata uint16_t crc_value;
extern __xdata struct flash_region_t flash_region;
extern __xdata uint8_t flash_buf[FLASH_BUF_SIZE];
void crc16_bank1(__xdata uint8_t *v) __naked;

void check_and_flash_update_image(void)
{
	flash_read_jedecid(); // This initializes also __xdata flash_size variable

	print_string(get_flash_size_str()); print_string(" flash size detected. (1 MB is needed for image updating)\n");
	if (flash_size < FIRMWARE_UPLOAD_START*2) {
		print_string("Flash too small for updating; skipping update check\n");
		return;
	}

	print_string("Checking for update image in flash... ");
	// Check if an update image is in flash
	flash_region.addr = FIRMWARE_UPLOAD_START;
	flash_region.len = 0x100;
	flash_read_bulk(flash_buf);
	if (flash_buf[0] == 0x00 && flash_buf[1] == 0x40)
	{
		// Yes, flash the new image to the start of flash and reset
		__xdata uint32_t dest = 0x0;
		__xdata uint32_t source = FIRMWARE_UPLOAD_START;
		__xdata uint16_t i = 0;
		__xdata uint16_t j = 0;
		__xdata uint8_t * __xdata bptr;
		print_string("found update image!\nChecking integrity");
		flash_init(0); // Re-initialize flash for non-DIO operation, otherwise flashing will fail
		set_sys_led_state(SYS_LED_FAST);
		crc_value = 0x0000;
		for (i = 0; i < 1024; i++) {
			flash_region.addr = source;
			flash_region.len = FLASH_BUF_SIZE;
			flash_read_bulk(flash_buf);
			bptr = flash_buf;
			for (j = 0; j < FLASH_BUF_SIZE; j++) {
				crc16_bank1(bptr++);
			}
			source += FLASH_BUF_SIZE;
			if (i%16 == 0) write_char('.');
		}
		if (crc_value == 0xb001) {
			print_string("Checksum OK.\nUpdate in progress, moving firmware to start of flash");
			source = FIRMWARE_UPLOAD_START;
			// Don't copy the config area at the end of flash
			for (i = 0; i < CONFIG_START/FLASH_BUF_SIZE; i++) {
				flash_region.addr = source;
				flash_region.len = FLASH_BUF_SIZE;
				flash_read_bulk(flash_buf);
				if (i%8 == 0) {
					flash_region.addr = dest;
					flash_sector_erase();
					if (i%16 == 0) write_char('.');
				}
				flash_region.addr = dest;
				flash_region.len = FLASH_BUF_SIZE;
				flash_write_bytes(flash_buf);
				dest += FLASH_BUF_SIZE;
				source += FLASH_BUF_SIZE;
			}
			print_string("Done.\nDeleting uploaded flash image");
			dest = FIRMWARE_UPLOAD_START;
			for (register uint8_t i=0; i < 128; i++) // TODO: Erasing the entire 512kByte upload area is probably not necessary
			{
				flash_region.addr = dest;
				flash_sector_erase();
				dest += 0x1000;
				if (i%4 == 0) write_char('.');
			}
			print_string("Done.\nResetting now");
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
	else
	{
		print_string("no update image found.\n");
	}
}

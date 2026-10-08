// Applies a staged image, see update_apply.h
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_flash.h"
#include "update_pool.h"
#include "update_apply.h"

extern __xdata uint16_t crc_value;
void crc16_bank1(__xdata uint8_t *v) __naked;
extern __xdata uint8_t flash_buf[FLASH_BUF_SIZE];
extern __xdata struct flash_region_t flash_region;

#define SECTOR_BLOCKS (FLASH_SECTOR_SIZE / FLASH_BUF_SIZE)	// flash write granularity

// One block of a staged image sector into flash_buf: from the pool, else zeros
static void read_image_block(uint16_t sector, uint8_t block)
{
	__xdata uint16_t i;

	if (update_state.present[sector >> 3] & (1 << (sector & 7))) {
		flash_region.addr = update_pool_addr(update_pool_index(sector))
				    + (uint32_t)block * FLASH_BUF_SIZE;
		flash_region.len = FLASH_BUF_SIZE;
		flash_read_bulk(flash_buf);
		return;
	}
	for (i = 0; i < FLASH_BUF_SIZE; i++)
		flash_buf[i] = 0;
}

// CRC16 over the sectors the apply writes, as the upload computed it
static uint16_t staged_image_crc(void)
{
	__xdata uint16_t sector, i;
	__xdata uint8_t block;
	__xdata uint8_t * __xdata bptr;

	crc_value = 0;
	for (sector = 0; sector < UPDATE_APPLY_SECTORS; sector++) {
		for (block = 0; block < SECTOR_BLOCKS; block++) {
			read_image_block(sector, block);
			bptr = flash_buf;
			for (i = 0; i < FLASH_BUF_SIZE; i++)
				crc16_bank1(bptr++);
		}
		if ((sector & 0x0F) == 0)
			write_char('.');
	}
	return crc_value;
}

/*
 * Copies the staged sectors over the running image, highest first: sector 0
 * holds the prefetch header and is written last, so an interrupted copy still
 * boots and resumes. The sectors the image does not use follow as zeros.
 */
static void copy_staged_image(void)
{
	__xdata uint16_t sector;
	__xdata uint8_t block;
	__xdata uint32_t dest;

	for (sector = UPDATE_APPLY_SECTORS; sector > 0; sector--) {
		if (!(update_state.present[(sector - 1) >> 3] & (1 << ((sector - 1) & 7))))
			continue;
		dest = (uint32_t)(sector - 1) * FLASH_SECTOR_SIZE;
		for (block = 0; block < SECTOR_BLOCKS; block++) {
			read_image_block(sector - 1, block);
			if (!block) {
				flash_region.addr = dest;
				flash_sector_erase();
			}
			flash_region.addr = dest + (uint32_t)block * FLASH_BUF_SIZE;
			flash_region.len = FLASH_BUF_SIZE;
			flash_write_bytes(flash_buf);
		}
	}
}

// Zeros to the image sectors that were not staged, skipping the ones that
// already read zero; this also clears the staging pool
static void zero_unstaged_sectors(void)
{
	__xdata uint16_t sector, i;
	__xdata uint8_t block, used;
	__xdata uint8_t * __xdata bptr;
	__xdata uint32_t dest;

	for (sector = UPDATE_APPLY_SECTORS; sector > 0; sector--) {
		if (update_state.present[(sector - 1) >> 3] & (1 << ((sector - 1) & 7)))
			continue;
		dest = (uint32_t)(sector - 1) * FLASH_SECTOR_SIZE;
		used = 0;
		for (block = 0; block < SECTOR_BLOCKS; block++) {
			flash_region.addr = dest + (uint32_t)block * FLASH_BUF_SIZE;
			flash_region.len = FLASH_BUF_SIZE;
			flash_read_bulk(flash_buf);
			bptr = flash_buf;
			for (i = 0; i < FLASH_BUF_SIZE; i++) {
				if (*bptr++) {
					used = 1;
					break;
				}
			}
			if (used)
				break;
		}
		if (!used)
			continue;
		flash_region.addr = dest;
		flash_sector_erase();
		for (i = 0; i < FLASH_BUF_SIZE; i++)
			flash_buf[i] = 0;
		for (block = 0; block < SECTOR_BLOCKS; block++) {
			flash_region.addr = dest + (uint32_t)block * FLASH_BUF_SIZE;
			flash_region.len = FLASH_BUF_SIZE;
			flash_write_bytes(flash_buf);
		}
	}
}

/*
 * Applies a staged image: verify it, copy the staged sectors, zero the rest. The
 * state record resumes an interrupted run. Returns without touching the running
 * image when the staged one is unusable.
 */
static void apply_staged_image(void)
{
	update_pool_set_bottom(update_state.pool_bottom);
	if (!(update_state.flags & UPDATE_STATE_APPLY)) {
		print_string("Checking staged image");
		set_sys_led_state(SYS_LED_FAST);
		if (staged_image_crc() != update_state.crc_apply
		    || update_state.crc != IMAGE_CRC) {
			print_string("\nStaged image is damaged, discarding it\n");
			update_state_clear();
			return;
		}
		// an image without the prefetch header would boot a mix of both
		if (!(update_state.present[0] & 1)) {
			print_string("\nStaged image has no reset vector, discarding it\n");
			update_state_clear();
			return;
		}
		print_string("\n");
		// before this record a reset leaves the running image untouched
		update_state.flags |= UPDATE_STATE_APPLY;
		update_state_write();
	}
	if (!(update_state.flags & UPDATE_STATE_ZEROING)) {
		// the staged sectors are still in the pool here; copy them first
		print_string("Copying update into place\n");
		copy_staged_image();
		update_state.flags |= UPDATE_STATE_ZEROING;
		update_state_write();
	}
	print_string("Writing zeros to the rest of the image\n");
	zero_unstaged_sectors();
	update_state_clear();
	print_string("Update complete, resetting now\n");
	delay(200);
	reset_chip();
}

// Called at boot with a staged image in the pool
void update_apply_staged(void)
{
	if (update_state.flags & (UPDATE_STATE_APPLY | UPDATE_STATE_ZEROING))
		print_string("Update was interrupted by a reset, resuming\n");
	apply_staged_image();
	print_string("Continuing with the running image\n");
}

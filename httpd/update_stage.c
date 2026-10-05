/*
 * Sparse staging of a firmware image into the free flash of the running image,
 * see update_stage.h. The pool never covers the flash a code bank occupies, so
 * this code, which lives in BANK3, cannot erase itself.
 */
#include "rtl837x_common.h"
#include "rtl837x_flash.h"
#include "update_stage.h"

#pragma codeseg BANK3

extern __xdata uint8_t flash_buf[FLASH_BUF_SIZE];
extern __xdata struct flash_region_t flash_region;

// The image sector being received and how much of it is already staged
static __xdata uint16_t stage_sec;
static __xdata uint8_t stage_upage;	// 256 byte pages of it filled so far
static __xdata uint8_t stage_pages;	// pages of it already in the pool
static __xdata uint32_t stage_slot;	// pool address, 0 while the sector is empty
static __xdata uint32_t stage_bottom;	// start of the pool of this staging
static __xdata uint8_t stage_error;

/* Records a completely received image sector in the staging manifest. A sector
 * that never held anything is left out: the apply writes zeros there. */
static void stage_sector(void)
{
	if (stage_slot) {
		update_state.present[stage_sec >> 3] |= 1 << (stage_sec & 7);
		update_state.staged++;
	}
	stage_sec++;
	stage_upage = 0;
	stage_pages = 0;
	stage_slot = 0;
}

/*
 * Stages the 256-byte page just assembled in flash_buf. Returns UPDATE_STAGE_OK
 * when it was staged, UPDATE_STAGE_NO_ROOM when the free flash is used up and
 * UPDATE_STAGE_OVERLAP when staging it would sit on an address the copy back has
 * to write.
 */
static uint8_t stage_page(void)
{
	__xdata uint16_t i;
	__xdata uint8_t nonzero = 0;

	for (i = 0; i < FLASH_PAGE_SIZE; i++) {
		if (flash_buf[i]) {
			nonzero = 1;
			break;
		}
	}
	// Above the configuration the image is not applied, so not staged either
	if (stage_sec >= UPDATE_APPLY_SECTORS)
		return UPDATE_STAGE_OK;
	if (nonzero && !stage_slot) {
		/* Refuse before anything is written when this sector cannot be
		 * staged without the copy back spoiling the pool. */
		if (update_pool_target(stage_sec))
			return UPDATE_STAGE_OVERLAP;
		if (update_pool_conflict((uint32_t)stage_sec * FLASH_SECTOR_SIZE))
			return UPDATE_STAGE_OVERLAP;
		stage_slot = update_pool_addr(update_state.staged);
		if (!stage_slot)
			return UPDATE_STAGE_NO_ROOM;
		flash_region.addr = stage_slot;
		flash_sector_erase();
		stage_pages = 0;
	}
	if (!stage_slot)
		return UPDATE_STAGE_OK;	// an all-zero sector costs no pool space
	if (!nonzero) {
		/* A zero page inside a used sector has to be programmed: the
		 * slot was erased and the apply copies it back verbatim. */
		for (i = 0; i < FLASH_PAGE_SIZE; i++)
			flash_buf[i] = 0;
	}
	flash_region.addr = stage_slot + (uint32_t)stage_upage * FLASH_PAGE_SIZE;
	flash_region.len = FLASH_PAGE_SIZE;
	flash_write_bytes(flash_buf);
	if (stage_pages < stage_upage) {
		/* Pages that filled up while the sector still looked empty were
		 * dropped; they are part of the sector, so write them as zeros. */
		for (i = 0; i < FLASH_PAGE_SIZE; i++)
			flash_buf[i] = 0;
		for (i = stage_pages; i < stage_upage; i++) {
			flash_region.addr = stage_slot + (uint32_t)i * FLASH_PAGE_SIZE;
			flash_region.len = FLASH_PAGE_SIZE;
			flash_write_bytes(flash_buf);
		}
	}
	stage_pages = stage_upage + 1;
	return UPDATE_STAGE_OK;
}

void update_stage_begin(uint32_t pool_bottom) __banked
{
	stage_bottom = update_pool_set_bottom(pool_bottom);
	stage_sec = 0;
	stage_upage = 0;
	stage_pages = 0;
	stage_slot = 0;
	stage_error = 0;
	/* Invalidate the record of an earlier attempt: its staged sectors are
	 * overwritten from now on. */
	update_state_clear();
}

uint8_t update_stage_page(void) __banked
{
	if (stage_error)
		return stage_error;
	stage_error = stage_page();
	if (stage_error)
		return stage_error;
	if (++stage_upage == FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE)
		stage_sector();
	return UPDATE_STAGE_OK;
}

uint8_t update_stage_end(void) __banked
{
	__xdata uint16_t i;

	if (stage_error)
		return stage_error;
	/* Whole pages were staged, so a partially received sector is left over
	 * only when the body ended early, which fails the checksum. */
	if (stage_upage) {
		if (stage_slot) {
			for (i = 0; i < FLASH_PAGE_SIZE; i++)
				flash_buf[i] = 0;
			for (i = stage_pages; i < FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE; i++) {
				flash_region.addr = stage_slot + (uint32_t)i * FLASH_PAGE_SIZE;
				flash_region.len = FLASH_PAGE_SIZE;
				flash_write_bytes(flash_buf);
			}
		}
		stage_sector();
	}
	return UPDATE_STAGE_OK;
}

void update_stage_commit(uint16_t crc) __banked
{
	update_state.magic = UPDATE_STATE_MAGIC;
	update_state.flags = UPDATE_STATE_STAGING;
	update_state.crc = crc;
	/* The apply reads the staged sectors back through the same pool */
	update_state.pool_bottom = stage_bottom;
	update_state_write();
	stage_error = UPDATE_STAGE_OK;
}

void update_stage_discard(void) __banked
{
	update_state_clear();
}

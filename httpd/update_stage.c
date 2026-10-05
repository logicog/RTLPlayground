// Sparse staging into free flash, see update_stage.h
#include "rtl837x_common.h"
#include "rtl837x_flash.h"
#include "update_stage.h"

#pragma codeseg BANK3

extern __xdata uint8_t flash_buf[FLASH_BUF_SIZE];
extern __xdata struct flash_region_t flash_region;

// Sector being received, its filled pages and the pool slot allotted to it
static __xdata uint16_t stage_sec;
static __xdata uint8_t stage_upage;
static __xdata uint8_t stage_pages;
static __xdata uint32_t stage_slot;
static __xdata uint32_t stage_bottom;
static __xdata uint8_t stage_error;

// Records the sector just completed; an empty one is left out
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

// Stages the page in flash_buf, see update_stage.h for the return values
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
	// above the configuration nothing is applied, so nothing is staged
	if (stage_sec >= UPDATE_APPLY_SECTORS)
		return UPDATE_STAGE_OK;
	if (nonzero && !stage_slot) {
		// would the copy back write over the pool, or the pool over it?
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
		// the slot is erased, so zero pages of a used sector must be written
		for (i = 0; i < FLASH_PAGE_SIZE; i++)
			flash_buf[i] = 0;
	}
	flash_region.addr = stage_slot + (uint32_t)stage_upage * FLASH_PAGE_SIZE;
	flash_region.len = FLASH_PAGE_SIZE;
	flash_write_bytes(flash_buf);
	if (stage_pages < stage_upage) {
		// pages dropped while the sector still looked empty
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
	update_state_clear();	// the earlier attempt's record is stale now
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
	// a partial sector is left only by a body that ended early
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
	update_state.pool_bottom = stage_bottom;	// the apply reads the same pool
	update_state_write();
	stage_error = UPDATE_STAGE_OK;
}

void update_stage_discard(void) __banked
{
	update_state_clear();
}

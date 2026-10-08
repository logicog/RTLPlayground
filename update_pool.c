// Staging pool and the update state, see update_pool.h
#include "rtl837x_common.h"
#include "rtl837x_flash.h"
#include "update_pool.h"

extern __xdata struct flash_region_t flash_region;

__xdata update_state_t update_state;

void update_state_read(void)
{
	flash_region.addr = UPDATE_STATE_START;
	flash_region.len = sizeof(update_state);
	flash_read_bulk((__xdata uint8_t *)&update_state);
}

void update_state_write(void)
{
	flash_region.addr = UPDATE_STATE_START;
	flash_sector_erase();
	flash_region.addr = UPDATE_STATE_START;
	flash_region.len = sizeof(update_state);
	flash_write_bytes((__xdata uint8_t *)&update_state);
}

void update_state_clear(void)
{
	__xdata uint8_t i;

	update_state.magic = 0;
	update_state.flags = 0;
	update_state.staged = 0;
	update_state.crc = 0;
	update_state.crc_apply = 0;
	update_state.pool_bottom = 0;
	for (i = 0; i < sizeof(update_state.present); i++)
		update_state.present[i] = 0;
	update_state_write();
}

/*
 * Pool A is the flash between the code banks and the UI, which no build can
 * use, pool B the flash above the UI. Both fill from the top downwards.
 */
#define POOL_A_TOP	0x40000u
#define POOL_A_BOTTOM	0x28000u	// bank 3 ends here, see imagebuilder.c
#define POOL_B_TOP	DEFAULT_CONFIG_START
#define POOL_A_SECTORS	((POOL_A_TOP - POOL_A_BOTTOM) / FLASH_SECTOR_SIZE)

/*
 * Pool C is the flash between the live configuration and the update record,
 * which nothing else uses either. It lies above every address the copy writes,
 * so a slot in it never covers a target of the update that owns it; the copy's
 * zeroing pass clears it like the other two.
 */
#define POOL_C_TOP	UPDATE_STATE_START
#define POOL_C_BOTTOM	(CONFIG_START + CONFIG_LEN)
#define POOL_C_SECTORS	((POOL_C_TOP - POOL_C_BOTTOM) / FLASH_SECTOR_SIZE)

static __xdata uint32_t pool_bottom = POOL_A_TOP;

uint32_t update_pool_set_bottom(uint32_t bottom)
{
	// rounded up: a slot is a whole sector, the UI end is not sector aligned
	pool_bottom = (bottom + FLASH_SECTOR_SIZE - 1) & ~((uint32_t)FLASH_SECTOR_SIZE - 1);
	return pool_bottom;
}

uint32_t update_pool_addr(uint16_t staged_idx)
{
	if (staged_idx < POOL_A_SECTORS)
		return POOL_A_TOP - (uint32_t)(staged_idx + 1) * FLASH_SECTOR_SIZE;
	staged_idx -= POOL_A_SECTORS;
	if (pool_bottom < POOL_B_TOP) {
		__xdata uint16_t b_sectors =
			(uint16_t)((POOL_B_TOP - pool_bottom) / FLASH_SECTOR_SIZE);
		if (staged_idx < b_sectors)
			return POOL_B_TOP - (uint32_t)(staged_idx + 1) * FLASH_SECTOR_SIZE;
		staged_idx -= b_sectors;
	}
	if (staged_idx < POOL_C_SECTORS)
		return POOL_C_TOP - (uint32_t)(staged_idx + 1) * FLASH_SECTOR_SIZE;
	return 0;	// no room left
}

uint16_t update_pool_index(uint16_t sector)
{
	__xdata uint16_t i, n = 0;

	for (i = 0; i < sector; i++)
		if (update_state.present[i >> 3] & (1 << (i & 7)))
			n++;
	return n;
}

uint8_t update_pool_conflict(uint32_t addr)
{
	uint32_t end = addr + FLASH_SECTOR_SIZE;
	__xdata uint16_t n = update_state.staged;

	if (n > POOL_A_SECTORS) {
		uint32_t lo = POOL_B_TOP - (uint32_t)(n - POOL_A_SECTORS) * FLASH_SECTOR_SIZE;
		if (pool_bottom < POOL_B_TOP && addr < POOL_B_TOP && end > lo)
			return 1;
		n = POOL_A_SECTORS;
	}
	if (n) {
		uint32_t lo = POOL_A_TOP - (uint32_t)n * FLASH_SECTOR_SIZE;
		if (addr < POOL_A_TOP && end > lo)
			return 1;
	}
	return 0;
}

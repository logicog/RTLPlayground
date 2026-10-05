/* Flash model for the host tests, see flash_mock.h */
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_flash.h"
#include "flash_mock.h"

uint8_t flash_mock[FLASH_MOCK_SIZE];
uint32_t flash_mock_erases;
uint32_t flash_mock_writes;

extern __xdata struct flash_region_t flash_region;

static void flash_mock_nothing(void) { }
void (*flash_mock_after_write)(void) = flash_mock_nothing;

void flash_mock_reset(void)
{
	memset(flash_mock, 0xff, sizeof(flash_mock));
	flash_mock_erases = 0;
	flash_mock_writes = 0;
}

void flash_mock_load(uint32_t addr, const void *src, uint32_t len)
{
	memcpy(flash_mock + addr, src, len);
}

uint32_t flash_mock_nonzero(uint32_t addr, uint32_t len)
{
	uint32_t i, n = 0;

	for (i = 0; i < len; i++)
		if (flash_mock[addr + i])
			n++;
	return n;
}

/* The flash interface the firmware calls ---------------------------------- */

void flash_init(uint8_t enable_dio) { (void)enable_dio; }

void flash_read_bulk(uint8_t *dst)
{
	memcpy(dst, flash_mock + flash_region.addr, flash_region.len);
}

void flash_sector_erase(void)
{
	flash_mock_erases++;
	memset(flash_mock + (flash_region.addr & ~(FLASH_SECTOR_SIZE - 1)), 0xff,
	       FLASH_SECTOR_SIZE);
}

void flash_write_bytes(uint8_t *ptr)
{
	uint32_t addr = flash_region.addr;
	uint16_t len = flash_region.len;

	flash_mock_writes++;
	while (len) {
		uint32_t page = addr & ~(FLASH_PAGE_SIZE - 1);
		uint16_t n = len > 4 ? 4 : len;		/* the driver's command size */
		uint16_t i;

		for (i = 0; i < n; i++)
			flash_mock[page + ((addr - page + i) & (FLASH_PAGE_SIZE - 1))] &= ptr[i];
		ptr += n;
		addr += n;
		len -= n;
	}
	flash_mock_after_write();
}

const char *get_flash_size_str(void) { return "512 kB"; }

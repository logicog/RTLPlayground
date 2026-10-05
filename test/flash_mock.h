#ifndef _FLASH_MOCK_H_
#define _FLASH_MOCK_H_

#include <stdint.h>

/*
 * A flash the size of the largest part the firmware is used on, with the timing
 * removed. Erasing sets a sector to 0xff and programming only clears bits,
 * wrapping inside the 256-byte page the command started in, so a write that
 * skips an erase or crosses a page shows up instead of passing silently.
 */
#define FLASH_MOCK_SIZE	0x100000u

extern uint8_t flash_mock[FLASH_MOCK_SIZE];
extern uint32_t flash_mock_erases;
extern uint32_t flash_mock_writes;

// Called after every flash_write_bytes(); a test cuts the power here
extern void (*flash_mock_after_write)(void);

void flash_mock_reset(void);
void flash_mock_load(uint32_t addr, const void *src, uint32_t len);
uint32_t flash_mock_nonzero(uint32_t addr, uint32_t len);

#endif

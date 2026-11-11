/*
 * flash_dev.h - a faithful SPI-NOR flash device model for the host test bench.
 *
 * Models the properties that make flashing dangerous, so the update/boot-copy
 * logic can be tested natively without a programmer or a switch:
 *
 *   - erase sets a 4 KiB sector to 0xFF;
 *   - a write can only CLEAR bits (AND semantics); writing a bit that the
 *     cell does not hold erased is a "write without erase" fault;
 *   - the device has a fixed size, and addresses wrap at that size, exactly
 *     as the SoC's 24-bit SPI addressing does on a small chip - which is how
 *     a copy/erase that runs off the end silently lands back on the running
 *     image at address 0;
 *   - an operation count can be capped to model a power cut mid-update.
 *
 * It is pure: no firmware symbols. A separate shim binds the firmware's
 * flash_region / flash_read_bulk / flash_sector_erase / flash_write_bytes to
 * one of these.
 */
#ifndef FLASH_DEV_H
#define FLASH_DEV_H

#include <stdint.h>
#include <stddef.h>

#define FD_SECTOR_SIZE 0x1000u

typedef struct {
	uint8_t  *mem;		/* size bytes, 0xFF when erased */
	uint32_t  size;		/* power of two: 512K/1M/2M/4M */
	uint32_t  addr_mask;	/* size - 1; the SPI address wrap */

	/* observability for assertions */
	unsigned  erases;	/* sector erases performed */
	unsigned  writes;	/* write calls performed */
	unsigned  wrote_without_erase;	/* writes that tried to set a 0 bit */
	unsigned  wrapped_ops;	/* ops whose address wrapped past size */

	/* fault injection: once >0, each erase/write decrements it; at 0 the
	 * operation is dropped (power cut) and fd_powercut is set. */
	int       fail_after;	/* <0 = never fail */
	int       powercut;	/* set when a capped op was dropped */
} flashdev_t;

/* Allocate and erase (0xFF) a device of the given power-of-two size. */
void     fd_init(flashdev_t *fd, uint32_t size);
void     fd_free(flashdev_t *fd);

/* Preload content (e.g. a real .bin image), bypassing erase/write rules. */
void     fd_load(flashdev_t *fd, uint32_t addr, const uint8_t *src, uint32_t len);

/* Device operations. Addresses wrap at the device size, as the hardware does. */
void     fd_read(flashdev_t *fd, uint32_t addr, uint8_t *dst, uint32_t len);
void     fd_erase_sector(flashdev_t *fd, uint32_t addr);
void     fd_write(flashdev_t *fd, uint32_t addr, const uint8_t *src, uint32_t len);

/* Arm a power cut after n more erase/write operations (n<0 disarms). */
void     fd_fail_after(flashdev_t *fd, int n);

#endif

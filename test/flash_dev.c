/* flash_dev.c - see flash_dev.h. A faithful SPI-NOR model for host tests. */
#include "flash_dev.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int is_pow2(uint32_t v) { return v && !(v & (v - 1)); }

void fd_init(flashdev_t *fd, uint32_t size)
{
	assert(is_pow2(size));
	memset(fd, 0, sizeof(*fd));
	fd->mem = malloc(size);
	assert(fd->mem);
	memset(fd->mem, 0xFF, size);
	fd->size = size;
	fd->addr_mask = size - 1;
	fd->fail_after = -1;
}

void fd_free(flashdev_t *fd)
{
	free(fd->mem);
	fd->mem = NULL;
}

void fd_load(flashdev_t *fd, uint32_t addr, const uint8_t *src, uint32_t len)
{
	/* Preload is a programmer writing the chip: no erase/wrap accounting,
	 * but still honour the address space. */
	for (uint32_t i = 0; i < len; i++)
		fd->mem[(addr + i) & fd->addr_mask] = src[i];
}

/* Returns 1 if the operation should proceed, 0 if a power cut swallowed it. */
static int fault_gate(flashdev_t *fd)
{
	if (fd->fail_after < 0)
		return 1;
	if (fd->fail_after == 0) {
		fd->powercut = 1;
		return 0;
	}
	fd->fail_after--;
	return 1;
}

void fd_read(flashdev_t *fd, uint32_t addr, uint8_t *dst, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
		dst[i] = fd->mem[(addr + i) & fd->addr_mask];
}

void fd_erase_sector(flashdev_t *fd, uint32_t addr)
{
	if (!fault_gate(fd))
		return;
	fd->erases++;
	if (addr > fd->addr_mask)
		fd->wrapped_ops++;		/* ran off the end -> wrapped */
	uint32_t base = (addr & fd->addr_mask) & ~(FD_SECTOR_SIZE - 1);
	memset(fd->mem + base, 0xFF, FD_SECTOR_SIZE);
}

void fd_write(flashdev_t *fd, uint32_t addr, const uint8_t *src, uint32_t len)
{
	if (!fault_gate(fd))
		return;
	fd->writes++;
	if (addr > fd->addr_mask || addr + len > fd->size)
		fd->wrapped_ops++;
	for (uint32_t i = 0; i < len; i++) {
		uint32_t a = (addr + i) & fd->addr_mask;
		uint8_t  want = src[i], have = fd->mem[a];
		/* NOR can only clear bits: any bit set in want but clear in have
		 * cannot be written without erasing first. */
		if (want & ~have)
			fd->wrote_without_erase++;
		fd->mem[a] = have & want;
	}
}

void fd_fail_after(flashdev_t *fd, int n)
{
	fd->fail_after = n;
	fd->powercut = 0;
}

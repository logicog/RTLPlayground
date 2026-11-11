/*
 * test_flash_dev.c - unit test for the flash device model itself.
 *
 * Proves the model captures the SPI-NOR properties the update/boot-copy logic
 * depends on, before any firmware code is wired to it. Pure, no firmware.
 */
#include "flash_dev.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, msg) do { \
	if (cond) printf("PASS  %s\n", msg); \
	else { printf("FAIL  %s\n", msg); failures++; } \
} while (0)

int main(void)
{
	flashdev_t fd;

	/* fresh device is all 0xFF */
	fd_init(&fd, 0x80000);		/* 512 KiB */
	uint8_t b[8];
	fd_read(&fd, 0x1234, b, sizeof b);
	int all_ff = 1; for (unsigned i = 0; i < sizeof b; i++) all_ff &= (b[i] == 0xFF);
	CHECK(all_ff, "erased device reads 0xFF");

	/* write then read back */
	uint8_t data[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
	fd_write(&fd, 0x2000, data, 4);
	fd_read(&fd, 0x2000, b, 4);
	CHECK(memcmp(b, data, 4) == 0 && fd.wrote_without_erase == 0,
	      "write to erased sector reads back, no fault");

	/* write without erase: setting a bit that is clear is a fault */
	uint8_t over[1] = { 0xFF };
	fd_write(&fd, 0x2000, over, 1);		/* 0xFF over 0xDE needs bits set */
	CHECK(fd.wrote_without_erase == 1, "write that sets a cleared bit is flagged");
	fd_read(&fd, 0x2000, b, 1);
	CHECK(b[0] == 0xDE, "write-without-erase is AND (0xFF & 0xDE = 0xDE)");

	/* erase restores the whole sector to 0xFF */
	fd_erase_sector(&fd, 0x2abc);		/* sector 0x2000..0x2fff */
	fd_read(&fd, 0x2000, b, 4);
	all_ff = 1; for (unsigned i = 0; i < 4; i++) all_ff &= (b[i] == 0xFF);
	CHECK(all_ff && fd.erases == 1, "sector erase clears the whole 4 KiB sector");

	/* the headline: on a 512 KiB device, an op past the end wraps to 0 -
	 * exactly how a copy/clean-up that runs off the end destroys the image
	 * it just wrote at address 0. */
	fd_load(&fd, 0x00000, (const uint8_t *)"LIVE-IMAGE", 10);
	fd_erase_sector(&fd, 0x80000);		/* == 0x00000 on a 512 KiB chip */
	fd_read(&fd, 0x00000, b, 4);
	all_ff = 1; for (unsigned i = 0; i < 4; i++) all_ff &= (b[i] == 0xFF);
	CHECK(all_ff && fd.wrapped_ops == 1,
	      "erase past end wraps onto address 0 and is flagged (wrap-erase bug)");

	fd_free(&fd);

	/* power cut: cap ops, confirm the capped op is dropped */
	fd_init(&fd, 0x200000);			/* 2 MiB */
	fd_fail_after(&fd, 2);			/* allow 2 ops, drop the 3rd */
	fd_erase_sector(&fd, 0x1000);
	fd_write(&fd, 0x1000, data, 4);
	fd_write(&fd, 0x1004, data, 4);		/* 3rd op: dropped */
	fd_read(&fd, 0x1004, b, 4);
	all_ff = 1; for (unsigned i = 0; i < 4; i++) all_ff &= (b[i] == 0xFF);
	CHECK(fd.powercut && all_ff, "power cut drops the operation past the cap");
	fd_free(&fd);

	printf(failures ? "\nflash_dev: %d FAILURE(S)\n" : "\nflash_dev: ALL PASS\n", failures);
	return failures ? 1 : 0;
}

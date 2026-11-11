/*
 * test_update_apply.c - run the real check_and_flash_update_image() against the
 * modelled flash device, with real (or structurally-real) images.
 *
 * Covers: the 512 KiB "too small, refuse" gate; a valid image copied over the
 * running one with the config preserved and the upload area cleared; and a
 * power cut during the copy that the staged image lets a re-run complete.
 *
 * Optional arg: a path to a real built .bin; if given, it is also run through
 * the cycle and its CRC checked against the firmware's 0xB001 residual.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flash_dev.h"
#include "update_shim.h"

#define IMG       0x80000u	/* FIRMWARE_UPLOAD_START / standard image size */
#define CONFIG    0x70000u	/* CONFIG_START */

static int failures;
#define CHECK(cond, msg) do { \
	if (cond) printf("PASS  %s\n", msg); \
	else { printf("FAIL  %s\n", msg); failures++; } \
} while (0)

/* Build a 0x80000 image: starts 00 40, arbitrary body, last two bytes set so
 * the CRC-16 over the whole image is 0xB001 - exactly as a real build ends. */
static void make_valid_image(uint8_t *img)
{
	memset(img, 0, IMG);
	img[0] = 0x00; img[1] = 0x40;
	for (uint32_t i = 2; i < IMG - 2; i++)
		img[i] = (uint8_t)((i * 2654435761u) >> 24);
	uint16_t s = crc16_host_buf(img, IMG - 2);
	for (int b0 = 0; b0 < 256; b0++)
		for (int b1 = 0; b1 < 256; b1++)
			if (crc16_host_step(crc16_host_step(s, (uint8_t)b0), (uint8_t)b1) == 0xB001) {
				img[IMG - 2] = (uint8_t)b0;
				img[IMG - 1] = (uint8_t)b1;
				return;
			}
	fprintf(stderr, "make_valid_image: no CRC solution?!\n");
	exit(2);
}

/* Preload a device of `size` with a running image, a config marker, and a
 * staged image at IMG. The running image is 0x11; config is a recognisable
 * string; staged is `img`. */
static void setup(flashdev_t *fd, uint32_t size, const uint8_t *img)
{
	static uint8_t running[CONFIG];
	memset(running, 0x11, sizeof running);
	fd_init(fd, size);
	fd_load(fd, 0, running, sizeof running);
	fd_load(fd, CONFIG, (const uint8_t *)"CONFIG-KEEP", 11);
	if (img)
		fd_load(fd, IMG, img, IMG);
}

/* Assert the running area now equals the staged image and config survived. */
static int copied_ok(flashdev_t *fd, const uint8_t *img)
{
	for (uint32_t a = 0; a < CONFIG; a++)
		if (fd->mem[a] != img[a]) return 0;
	return memcmp(fd->mem + CONFIG, "CONFIG-KEEP", 11) == 0;
}

static void run_valid_cycle(uint32_t size, const char *label, const uint8_t *img)
{
	char msg[96];
	setup(&g_flash, size, img);
	int reset = update_run();
	snprintf(msg, sizeof msg, "%s: update applied (reset) and copied, config kept, no NOR fault", label);
	CHECK(reset && copied_ok(&g_flash, img)
	      && g_flash.wrote_without_erase == 0 && g_flash.wrapped_ops == 0
	      && g_flash.mem[IMG] == 0xFF /* upload area cleared */, msg);
	fd_free(&g_flash);
}

int main(int argc, char **argv)
{
	static uint8_t img[IMG];
	make_valid_image(img);

	/* sanity: our host CRC reproduces the firmware's residual on this image */
	CHECK(crc16_host_buf(img, IMG) == 0xB001, "host CRC over a valid image == 0xB001");

	/* (1) too small: a 512 KiB device refuses the upload, touches nothing */
	setup(&g_flash, 0x80000, img);
	int reset = update_run();
	CHECK(!reset && g_flash.writes == 0 && g_flash.erases == 0,
	      "512K device: update refused, flash untouched");
	fd_free(&g_flash);

	/* (2) valid image applied on the 1 MiB minimum and on 2 MiB */
	run_valid_cycle(0x100000, "1M", img);
	run_valid_cycle(0x200000, "2M", img);

	/* (3) power cut partway through the copy; the staged image is intact, so
	 * a re-run completes. */
	setup(&g_flash, 0x200000, img);
	fd_fail_after(&g_flash, 20);		/* die after 20 erase/write ops */
	reset = update_run();
	CHECK(!reset && g_flash.powercut && !copied_ok(&g_flash, img),
	      "power cut mid-copy: not applied, copy incomplete");
	fd_fail_after(&g_flash, -1);		/* power restored */
	reset = update_run();
	CHECK(reset && copied_ok(&g_flash, img),
	      "re-run after power cut completes from the intact staged image");
	fd_free(&g_flash);

	/* (optional) a real built image */
	if (argc > 1) {
		FILE *f = fopen(argv[1], "rb");
		if (!f) { perror(argv[1]); return 2; }
		static uint8_t real[IMG];
		size_t n = fread(real, 1, IMG, f); fclose(f);
		if (n != IMG) { fprintf(stderr, "%s: expected %u bytes, got %zu\n", argv[1], IMG, n); return 2; }
		CHECK(crc16_host_buf(real, IMG) == 0xB001, "real image: CRC == 0xB001 (matches crc_calculator)");
		run_valid_cycle(0x200000, "2M real image", real);
	}

	printf(failures ? "\ntest_update_apply: %d FAILURE(S)\n" : "\ntest_update_apply: ALL PASS\n", failures);
	return failures ? 1 : 0;
}

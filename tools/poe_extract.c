/*
 * poe_extract - lift the PSE controller firmware out of a vendor switch image
 * and write it as a C array for the poe/ driver the build selected.
 *
 * The vendor keeps the PSE image in the switch firmware's own flash and
 * downloads it over I2C at boot. In REALHD_PSW8_25MG.bin the images live
 * back to back in code bank 22 (RTL8238B at 0x1002E6 with 0x52AC bytes,
 * RTL8238C at 0x105592 with 0x1F5C bytes), but their position depends on the
 * firmware version, so the tool locates them by content rather than by a
 * recorded offset:
 *
 * Every image is a self-describing container with a 16-byte little-endian
 * header:
 *
 *   0..1   two zero bytes             }
 *   2..3   signature 0x8239           } the unique search pattern
 *   4..5   per-chip id (0x000f 8238B / 0x000b 8238C)
 *   6..13  four u16 section lengths   } the payload, section 0 first
 *   14..15 two zero bytes
 *
 * followed by the payload the four sections describe, followed by a 4-byte
 * checksum chosen so that the whole container - read as little-endian 32-bit
 * words - sums to zero: the trailer is the two's complement of the sum of
 * everything before it. The header therefore also fixes the image size,
 *
 *   image_len == 16 + seclen[0] + seclen[1] + seclen[2] + seclen[3] + 4
 *
 * so the tool takes the length from the image itself and checks what it
 * extracted instead of trusting a size recorded here: a container whose header
 * and checksum do not add up is refused rather than linked into the firmware.
 *
 * The tool refuses to extract when the header is missing or occurs more than
 * once, when the sections describe more than the payload bound or more than
 * what is left of the vendor image, or when the checksum does not match,
 * rather than guessing which match is meant.
 *
 * The output is generated vendor firmware: it stays in the build tree (see
 * .gitignore) and must not be committed.
 */
#include <argp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_LEN	16	/* container header, described above */
#define PATTERN_LEN	6	/* leading bytes that identify the container */
#define TRAILER_LEN	4	/* checksum behind the payload */

/* The chip's loader assembles the payload in its own memory, so the four
 * sections may not describe more than 64 KiB between them. The tighter bound
 * comes from the driver: it passes the size of the whole container - 20 bytes
 * more than the payload - on as a uint16_t, so the payload stops one byte below
 * 64 KiB (65535 - 16 - 4). */
#define SECTION_TOTAL_MAX	(UINT16_MAX - HEADER_LEN - TRAILER_LEN)

struct image {
	const char *chip;
	uint8_t pattern[PATTERN_LEN];	/* start of the container, as found in the file */
	const char *vendor_action;
};

/* The image as its own header describes it. */
struct container {
	uint16_t section[4];	/* the four section lengths */
	uint32_t sections;	/* their sum: the payload size */
	uint32_t size;		/* header + payload + trailer */
};

static const struct image images[] = {
	{ "RTL8238C", { 0x00, 0x00, 0x39, 0x82, 0x0b, 0x00 }, "load 8238C pse image" },
};

struct arguments {
	const char *input;
	const char *chip;
	const char *output;
};

static char doc[] =
	"Extract a PSE controller firmware image from a vendor switch image\n"
	"and write it as a C array for the RTLPlayground poe/ driver.";
static char args_doc[] = "";
static struct argp_option options[] = {
	{ .name = "input",  .key = 'i', .arg = "FILE",
	  .doc = "Vendor switch firmware image to read" },
	{ .name = "chip",   .key = 'c', .arg = "CHIP",
	  .doc = "PSE chip named by MACHINE_POE_CHIP (RTL8238C)" },
	{ .name = "output", .key = 'o', .arg = "FILE",
	  .doc = "Write the generated C source here" },
	{ 0 }
};
const char *argp_program_version = "poe_extract 0.3";


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case 'i':
		arguments->input = arg;
		break;
	case 'c':
		arguments->chip = arg;
		break;
	case 'o':
		arguments->output = arg;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };


static const struct image *find_image(const char *chip)
{
	for (size_t i = 0; i < sizeof(images) / sizeof(images[0]); i++)
		if (!strcmp(images[i].chip, chip))
			return &images[i];
	return NULL;
}


static uint8_t *read_file(const char *name, size_t *size)
{
	FILE *f = fopen(name, "rb");
	uint8_t *buf;
	long len;

	if (!f) {
		perror(name);
		return NULL;
	}
	if (fseek(f, 0, SEEK_END) || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
		perror(name);
		fclose(f);
		return NULL;
	}
	buf = malloc((size_t)len);
	if (!buf) {
		fprintf(stderr, "%s: out of memory\n", name);
		fclose(f);
		return NULL;
	}
	if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
		fprintf(stderr, "%s: short read\n", name);
		free(buf);
		fclose(f);
		return NULL;
	}
	fclose(f);
	*size = (size_t)len;
	return buf;
}


static void print_pattern(char *buf, size_t len, const struct image *img)
{
	snprintf(buf, len, "%02x %02x %02x %02x %02x %02x",
		 img->pattern[0], img->pattern[1], img->pattern[2],
		 img->pattern[3], img->pattern[4], img->pattern[5]);
}


/* Find the single occurrence of the image header in the vendor firmware. */
static int locate_image(const char *name, const uint8_t *data, size_t size,
			const struct image *img, size_t *offset)
{
	char pattern[32];
	size_t found = 0;
	size_t first = 0;

	for (size_t i = 0; i + PATTERN_LEN <= size; i++) {
		if (memcmp(data + i, img->pattern, PATTERN_LEN))
			continue;
		if (!found)
			first = i;
		found++;
	}

	print_pattern(pattern, sizeof(pattern), img);
	if (!found) {
		fprintf(stderr, "%s: no %s image header (%s) found\n",
			name, img->chip, pattern);
		return 1;
	}
	if (found > 1) {
		fprintf(stderr,
			"%s: ambiguous, %zu %s image headers (%s), first at 0x%zx\n",
			name, found, img->chip, pattern, first);
		return 1;
	}

	*offset = first;
	return 0;
}


static uint16_t rd16(const uint8_t *p)
{
	return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}


static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}


/*
 * Read the container's self-describing header and check that the image it
 * describes is complete, within bounds and intact. The size comes from the
 * header - 16 + the four sections + 4 - and everything is checked against what
 * the vendor image actually holds, so a truncated or damaged container is
 * refused instead of being linked into the firmware.
 */
static int parse_container(const char *name, const char *chip, const uint8_t *data,
			   size_t size, size_t offset, struct container *ctr)
{
	const uint8_t *c = data + offset;
	size_t remaining = size - offset;
	uint32_t sum = 0;
	uint32_t trailer;
	uint32_t expected;

	ctr->sections = 0;
	for (int i = 0; i < 4; i++) {
		ctr->section[i] = rd16(c + 6 + 2 * i);
		ctr->sections += ctr->section[i];
	}
	ctr->size = HEADER_LEN + ctr->sections + TRAILER_LEN;

	if (c[14] || c[15]) {
		fprintf(stderr, "%s: %s image at 0x%zx has a non-zero header pad (%02x %02x)\n",
			name, chip, offset, c[14], c[15]);
		return 1;
	}
	if (ctr->sections > SECTION_TOTAL_MAX) {
		fprintf(stderr,
			"%s: %s image at 0x%zx describes %u bytes in its four sections, over the %u byte bound\n",
			name, chip, offset, ctr->sections, SECTION_TOTAL_MAX);
		return 1;
	}
	if (ctr->size > remaining) {
		fprintf(stderr,
			"%s: %s image at 0x%zx is %u bytes (16 + %u + 4), only %zu bytes left in the file\n",
			name, chip, offset, ctr->size, ctr->sections, remaining);
		return 1;
	}
	if (ctr->size % 4) {
		fprintf(stderr,
			"%s: %s image at 0x%zx is %u bytes, not a multiple of 4, so it cannot hold its dword checksum\n",
			name, chip, offset, ctr->size);
		return 1;
	}

	/* The trailer makes the container sum to zero over its little-endian
	 * 32-bit words, i.e. it is the two's complement of the sum before it. */
	for (uint32_t i = 0; i < ctr->size - TRAILER_LEN; i += 4)
		sum += rd32(c + i);
	trailer = rd32(c + ctr->size - TRAILER_LEN);
	expected = 0 - sum;
	if (trailer != expected) {
		fprintf(stderr,
			"%s: %s image at 0x%zx (%u bytes) fails its checksum: trailer %08x, expected %08x\n",
			name, chip, offset, ctr->size, trailer, expected);
		return 1;
	}

	return 0;
}


static int write_c(const char *path, const char *input, const struct image *img,
		   size_t offset, const uint8_t *data, const struct container *ctr)
{
	char tmp[4096];
	FILE *out;

	if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) {
		fprintf(stderr, "%s: path too long\n", path);
		return 1;
	}
	out = fopen(tmp, "w");
	if (!out) {
		perror(tmp);
		return 1;
	}

	fprintf(out,
		"/*\n"
		" * Generated by tools/poe_extract from %s - do not edit.\n"
		" *\n"
		" * %s PSE firmware image (%u bytes, found at file offset 0x%zx by\n"
		" * header scan; sections %u+%u+%u+%u = %u bytes, checksum verified;\n"
		" * vendor loader message \"%s\").\n"
		" *\n"
		" * The vendor keeps this image in the switch firmware and downloads it\n"
		" * over I2C at boot. It is vendor property and must not be committed;\n"
		" * the build generates it from stock_fw/<machine>.bin.\n"
		" */\n\n"
		"#include <stdint.h>\n\n"
		"/* Must share a bank with the driver that reads it: the 8051 address\n"
		" * space selects the code bank at runtime, and the driver's banked calls\n"
		" * leave PSBANK on this bank. */\n"
		"#pragma codeseg BANK3\n"
		"#pragma constseg BANK3\n\n"
		"__code const uint8_t poe_firmware[] = {\n",
		input, img->chip, ctr->size, offset,
		ctr->section[0], ctr->section[1], ctr->section[2], ctr->section[3],
		ctr->sections, img->vendor_action);

	for (uint32_t i = 0; i < ctr->size; i++) {
		if (!(i % 12))
			fprintf(out, "\t");
		fprintf(out, "0x%02x,", data[offset + i]);
		if (i % 12 == 11 || i + 1 == ctr->size)
			fprintf(out, "\n");
		else
			fprintf(out, " ");
	}

	fprintf(out,
		"};\n\n"
		"__code const uint16_t poe_firmware_len = %u;\n",
		ctr->size);

	if (fclose(out)) {
		perror(tmp);
		remove(tmp);
		return 1;
	}
	if (rename(tmp, path)) {
		perror(path);
		remove(tmp);
		return 1;
	}
	return 0;
}


int main(int argc, char **argv)
{
	struct arguments arguments = { 0 };
	struct container ctr;
	const struct image *img;
	uint8_t *data;
	size_t size;
	size_t offset;
	int index = 0;
	int ret;

	argp_parse(&argp, argc, argv, 0, &index, &arguments);

	if (!arguments.input || !arguments.chip || !arguments.output) {
		fprintf(stderr, "usage: %s -i <vendor image> -c <chip> -o <output.c>\n",
			argv[0]);
		return 2;
	}

	img = find_image(arguments.chip);
	if (!img) {
		fprintf(stderr, "%s: no firmware image known for this chip\n",
			arguments.chip);
		return 2;
	}

	data = read_file(arguments.input, &size);
	if (!data)
		return 1;

	if (locate_image(arguments.input, data, size, img, &offset)) {
		free(data);
		return 1;
	}
	if (parse_container(arguments.input, img->chip, data, size, offset, &ctr)) {
		free(data);
		return 1;
	}
	printf("Found %s PSE image at 0x%zx (%u bytes, sections %u+%u+%u+%u, checksum OK)\n",
	       img->chip, offset, ctr.size, ctr.section[0], ctr.section[1],
	       ctr.section[2], ctr.section[3]);

	ret = write_c(arguments.output, arguments.input, img, offset, data, &ctr);
	free(data);
	return ret;
}

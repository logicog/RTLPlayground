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
 * Every image begins with a six-byte container header: two zero bytes, the
 * little-endian signature 0x8239 and a per-chip u16 (0x000f for 8238B, 0x000b
 * for 8238C). The header is part of the image - the 8238B one ends exactly
 * where the 8238C one starts - and its whole six bytes are unique in the
 * vendor image, so they are used as the search pattern. The container carries
 * no length field, so the size of each image stays a property of the chip
 * entry below. The tool refuses to extract when the header is missing or
 * occurs more than once, rather than guessing which match is meant.
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

#define HEADER_LEN 6

struct image {
	const char *chip;
	uint8_t header[HEADER_LEN];	/* start of the image, as found in the file */
	uint32_t length;	/* image size including the header, not stored in it */
	const char *vendor_action;
};

static const struct image images[] = {
	{ "RTL8238C", { 0x00, 0x00, 0x39, 0x82, 0x0b, 0x00 }, 0x1f5cu,
	  "load 8238C pse image" },
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
const char *argp_program_version = "poe_extract 0.2";


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


static void print_header(char *buf, size_t len, const struct image *img)
{
	snprintf(buf, len, "%02x %02x %02x %02x %02x %02x",
		 img->header[0], img->header[1], img->header[2],
		 img->header[3], img->header[4], img->header[5]);
}


/* Find the single occurrence of the image header in the vendor firmware. */
static int locate_image(const char *name, const uint8_t *data, size_t size,
			const struct image *img, size_t *offset)
{
	char header[32];
	size_t found = 0;
	size_t first = 0;

	for (size_t i = 0; i + HEADER_LEN <= size; i++) {
		if (memcmp(data + i, img->header, HEADER_LEN))
			continue;
		if (!found)
			first = i;
		found++;
	}

	print_header(header, sizeof(header), img);
	if (!found) {
		fprintf(stderr, "%s: no %s image header (%s) found\n",
			name, img->chip, header);
		return 1;
	}
	if (found > 1) {
		fprintf(stderr,
			"%s: ambiguous, %zu %s image headers (%s), first at 0x%zx\n",
			name, found, img->chip, header, first);
		return 1;
	}

	*offset = first;
	return 0;
}


static int write_c(const char *path, const char *input, const struct image *img,
		   size_t offset, const uint8_t *data)
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
		" * header scan, vendor loader message \"%s\").\n"
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
		input, img->chip, img->length, offset, img->vendor_action);

	for (uint32_t i = 0; i < img->length; i++) {
		if (!(i % 12))
			fprintf(out, "\t");
		fprintf(out, "0x%02x,", data[offset + i]);
		if (i % 12 == 11 || i + 1 == img->length)
			fprintf(out, "\n");
		else
			fprintf(out, " ");
	}

	fprintf(out,
		"};\n\n"
		"__code const uint16_t poe_firmware_len = %u;\n",
		img->length);

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
	if ((uint64_t)offset + img->length > size) {
		fprintf(stderr,
			"%s: %s image at 0x%zx is truncated (%u bytes, %zu left)\n",
			arguments.input, img->chip, offset, img->length,
			size - offset);
		free(data);
		return 1;
	}
	printf("Found %s PSE image at 0x%zx (%u bytes)\n",
		img->chip, offset, img->length);

	ret = write_c(arguments.output, arguments.input, img, offset, data);
	free(data);
	return ret;
}

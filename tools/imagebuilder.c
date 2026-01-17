/*
 * Adds data files into specified locations of an image, optionally creates
 * an index in the form of a header file
 */
#include <stdint.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <stdbool.h>


#define OFFSET 2
#define BANK0_SIZE 0x4000
#define BANK_SIZE 0xc000
#define BANK_STRIDE 0x10000

// Use a 4MB buffer, the maximum flash rom size
#define BUFFER_SIZE 0x400000
uint8_t buffer[BUFFER_SIZE];
FILE *inptr;
int outptr;

const char *argp_program_version = "imagebuilder 0.1";
const char *argp_program_bug_address = "<git@logicog.de>";
static char doc[] = "Create an image with multiple banks for RTL837X-based switches";
static char args_doc[] = "INPUT_IMAGE";
static struct argp_option options[] = {
    { "input", 'i', "FILE", 0, "Input file"},
    { 0 }
};


struct arguments {
        char *input_file;
};


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
	switch (key) {
	case 'i':
		arguments->input_file = arg;
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 1)  // Expect 1 command line argument at end
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


static struct argp argp = {
	options, parse_opt, args_doc, doc, 0, 0, 0
};


int main(int argc, char **argv)
{
	struct arguments arguments;
	int arg_index;

	arguments.input_file = NULL;

	argp_parse(&argp, argc, argv, 0, &arg_index, &arguments);

	memset(buffer, 0, BUFFER_SIZE);

	size_t filesize = 0;
	inptr = fopen(arguments.input_file, "rb");
	if (inptr == NULL) {
		printf("Cannot open input file %s\n", arguments.input_file);
		return 5;
	}

	fseek(inptr, 0L, SEEK_END);
	filesize = ftell(inptr);
	rewind(inptr);
	printf("Input file size: %ld\n", filesize);
	if (filesize > BUFFER_SIZE) {
		printf("File too large.\n");
		return 5;
	}

	int nbanks = (filesize - BANK0_SIZE) / BANK_STRIDE;
	printf("Input file contains %d banks\n", nbanks);

	/* Verify the size of banks in input file:
	 * Bank 0: 0x00002 - 0x04000 <- 0x00000 - 0x03ffe
	 * Bank 1: 0x04000 - 0x10000 <- 0x14000 - 0x20000
	 * Bank 2: 0x10000 - 0x1c000 <- 0x24000 - 0x33000
	 * [...]
	 * by verifying whether other than 0-bytes are found in the input
	 * file after these ranges.
	 */
	size_t  bytes_read = fread(buffer, 1, filesize, inptr);
	if (bytes_read != filesize) {
			printf("Error reading input file.\n");
			return 5;
	}
	for (int b = BANK0_SIZE; b < BANK_STRIDE + BANK0_SIZE; b ++) {
		if (buffer[b]) {
			printf("WARNING: Bank 0: code segment too large at 0x%x!\n", b);
			break;
		}
	}
	for (int bank = 1; bank <= nbanks; bank++) {
		for (int b = (bank + 1) * BANK_STRIDE; b < (bank + 1) * BANK_STRIDE + BANK0_SIZE; b ++) {
			if (buffer[b]) {
				printf("WARNING: Bank %d: code segment too large at 0x%x!\n", bank, b);
				break;
			}
		}
	}

	rewind(inptr);
	memset(buffer, 0, BUFFER_SIZE);

	// BANK0-Size
	buffer[0] = 0x00;
	buffer[1] = 0x40;

	// Read bank 0
	bytes_read = fread(buffer + OFFSET, 1, BANK0_SIZE, inptr);
	printf("Bank 0: Bytes read: %ld\n", bytes_read);

	if (bytes_read != BANK0_SIZE) {
		printf("Error reading input file.\n");
		return 5;
	}
	fseek(inptr, BANK_STRIDE, SEEK_CUR);

	for (int bank = 1; bank <= nbanks; bank++) {
		bytes_read = fread(buffer + (bank - 1) * BANK_SIZE + BANK0_SIZE, 1, BANK_SIZE, inptr);
		printf("Bank %d: bytes read: %ld\n", bank, bytes_read);
		if (bank != nbanks)
			fseek(inptr, BANK0_SIZE, SEEK_CUR);
	}

	fclose(inptr);

	outptr = creat(argv[arg_index], S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

	if (!outptr) {
		printf("Cannot open %s\n", argv[arg_index]);
		return 5;
	}

	size_t written = write(outptr, buffer, (nbanks + 1) * BANK_STRIDE);

	if (written != (nbanks + 1) * BANK_STRIDE) {
		printf("Error writing output file.\n");
		return 5;
	}
	close(outptr);

	return 0;
}

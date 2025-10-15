/*
 * Calculator for the CRC16 as described in AN27 by Dallas Semiconductor
 * http://www.microshadow.com/files/files8051/app27.pdf
 * The implementation in C is based on the code given in
 * https://carta.tech/man-pages/man3/_crc_ibutton_update.3avr.html
 * The polynomial of the CRC is 0xa001: x^16 + x^15 + x^2 + 1
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <argp.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>


// Use a 4MB buffer, the same as the flash rom size
#define BUFFER_SIZE 0x400000
char buffer[BUFFER_SIZE];

struct arguments {
	char *input_file;
	char *output_file;
	bool update;
	bool verify;
};

const char *argp_program_version = "crc_calculator 0.1";
const char *argp_program_bug_address = "https://github.com/logicog/RTLPlayground/issues";
static char doc[] = "Calculate (and optionally update) the CRC of an image";
static char args_doc[] = "crc_calculator [options] INPUT_IMAGE";
static struct argp_option options[] = {
    { "output", 'o', "FILE", 0, "Output image file name instead of overwriting input image"},
    { "update", 'u', 0, OPTION_ARG_OPTIONAL, "Update the image with the CRC"},
    { "verify", 'v', 0, OPTION_ARG_OPTIONAL, "Verify the CRC of the file"},
    { 0 }
};


uint16_t crc16_update(uint16_t crc, uint8_t a)
{
    crc ^= a;
    for (int i = 0; i < 8; ++i)
	    crc = crc & 1 ? (crc >> 1) ^ 0xA001 : crc >> 1;

    return crc;
}


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
	switch (key) {
	case 'u':
		arguments->update = true;
		break;
	case 'v':
		arguments->verify = true;
		break;
	case 'o':
		arguments->output_file = arg;
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
	char tmpfilename[] = "crc_XXXXXX";
	uint16_t crc = 0;
	FILE *inptr;
	int outptr;
	int range;

	arguments.input_file = NULL;
	arguments.output_file = NULL;
	arguments.update = false;
	arguments.verify = false;

	argp_parse(&argp, argc, argv, 0, &arg_index, &arguments);
	if (!arg_index)
		argp_usage (0);

	memset(buffer, 0, BUFFER_SIZE);

	size_t filesize = 0;
	if (argv[arg_index]) {
		inptr = fopen(argv[arg_index], "rb");
		if (inptr == NULL) {
			printf("Cannot open input file %s\n", argv[arg_index]);
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
		size_t bytes_read = fread(buffer, 1, sizeof(buffer), inptr);

		printf("Bytes read: %ld\n", bytes_read);

		if (bytes_read != filesize) {
			printf("Error reading input file.\n");
			return 5;
		}
		fclose(inptr);
	}

	range = filesize;
	if (!arguments.verify) {
		if (arguments.update) {
			range -= 2;
		} else if (arguments.output_file) {
			filesize += 2;
		}
	}

	for (int i = 0; i < range; i++)
		crc = crc16_update(crc, buffer[i]);

	printf("CRC16 is: 0x%04x\n", crc);

	if (arguments.verify) {
		if (crc == 0xb001) {
			printf("Checksum OK\n");
			return 0;
		} else {
			printf("Checksum Incorrect\n");
			return 5;
		}
	}

	// We have to create a new image with updated CRC
	if (arguments.update || arguments.output_file) {
		crc ^= 0xffff;
		printf("Setting CRC bytes at position: 0x%x to CRC 1s complement 0x%04x\n", range, crc);
		// The CRC algorithm expects as input always first the LO-Byte
		buffer[range] = crc;
		buffer[range + 1] = crc >> 8;

		if (!arguments.update)
			outptr = creat(arguments.output_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		else
			outptr = mkstemp(tmpfilename);

		if (!outptr) {
			printf("Cannot open %s\n", arguments.output_file);
			return 5;
		}
		size_t written = write(outptr, buffer, filesize);

		if (written != filesize) {
			printf("Error writing output file.\n");
			return 5;
		}
		close(outptr);

		if (arguments.update)
			rename(tmpfilename, argv[arg_index]);
	}

	return 0;
}

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

#define HEADER_LENGTH 0x14
#define HEADER_MAGIC 0x12345678
#define HEADER_RESERVED 0x332255ff

#define SEG_01002_LENGTH 0x2ffe
#define SEG_1C000_LENGTH 0x1000
#define SEG_1d000_OFFSET 0x3ffe


// Use a 4MB buffer, the maximum flash rom size
#define BUFFER_SIZE 0x400000
uint8_t buffer[BUFFER_SIZE];
FILE *inptr;
int outptr;
char line[256];

const char *argp_program_version = "updatebuilder 0.1";
const char *argp_program_bug_address = "<git@logicog.de>";
static char doc[] = "Create an update image for RTL837X-based switches";
static char args_doc[] = "INPUT_IMAGE";
static struct argp_option options[] = {
    { "magic", 'm', "MAGIC", 0, "Magic number"},
    { "reserved", 'r', "MAGIC", 0, "Reserved number"},
    { "installer", 'i', "FILE", 0, "Installer file"},
    { "output", 'o', "FILE", 0, "Output file"},
    { 0 }
};


struct arguments {
	uint32_t reserved;
	uint32_t magic;
        char *installer_file;
        char *output_file;
};


int get_byte(int pos)
{
	int c1 = line[pos];
	if (c1 >= '0' && c1 <= '9')
		c1 -= '0';
	else if (c1 >= 'a' && c1 <= 'f')
		c1 = c1 - 'a' + 10;
	else if (c1 >= 'A' && c1 <= 'F')
		c1 = c1 - 'A' + 10;
	else
		return -1;

	int c2 = line[pos + 1];
	if (c2 >= '0' && c2 <= '9')
		c2 -= '0';
	else if (c2 >= 'a' && c2 <= 'f')
		c2 = c2 - 'a' + 10;
	else if (c2 >= 'A' && c2 <= 'F')
		c2 = c2 - 'A' + 10;
	else
		return -1;

	return c1 << 4 | c2;
}


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
	switch (key) {
	case 'm':
		arguments->magic = arg? strtol(arg, NULL, 16): HEADER_MAGIC;
		break;
	case 'r':
		arguments->reserved = arg? strtol(arg, NULL, 16): HEADER_RESERVED;
		break;
	case 'i':
		arguments->installer_file = arg;
		break;
	case 'o':
		arguments->output_file = arg;
		break;
	case ARGP_KEY_END:
		if(state->arg_num < 1)  // Expect 1 command line argument at end
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
	char tmpfilename[] = "image_XXXXXX";

	arguments.reserved = HEADER_RESERVED;
	arguments.magic = HEADER_MAGIC;
	arguments.output_file = NULL;

	argp_parse(&argp, argc, argv, 0, &arg_index, &arguments);

	memset(buffer, 0, BUFFER_SIZE);

	size_t filesize = 0;
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
	size_t bytes_read = fread(buffer + SEG_1d000_OFFSET + 2 * HEADER_LENGTH, 1, sizeof(buffer), inptr);

	printf("Bytes read: %ld\n", bytes_read);

	if (bytes_read != filesize) {
		printf("Error reading input file.\n");
		return 5;
	}
	fclose(inptr);
	filesize += SEG_1d000_OFFSET + 2 * HEADER_LENGTH;

	// Read the installer file, which is in Intel Hex format
	if (arguments.installer_file) {
		inptr = fopen(arguments.installer_file, "rb");
		if (inptr == NULL) {
			printf("Cannot open installer file %s\n", arguments.installer_file);
			return 5;
		}
		int line_num = 1;
		int address_high = 0;
		bool eof = false;
		while(fgets(line, 255, inptr)) {
			if (line[0] != ':') {
				printf("Unknwon installer file format for %s\n", arguments.installer_file);
				return 5;
			}
			int bytes = get_byte(1);
			if (bytes < 0 || bytes > 200) {
				printf("Error in %s, line %d, incorrect byte number\n", arguments.installer_file, line_num);
				return 5;
			}
			int address = (get_byte(3) *256) + get_byte(5);
			if (address < 0) {
				printf("Error in %s, line %d, not an address\n", arguments.installer_file, line_num);
				return 5;
			}
			int type = get_byte(7);
			if (type < 0 || type > 5) {
				printf("Error in %s, line %d incorrect type\n", arguments.installer_file, line_num);
				return 5;
			}
			if (type == 0) {
				for (int i = 0; i < bytes; i++) {
					int data = get_byte(9 + 2 * i);
					if (data < 0) {
						printf("Error in %s, line %d, illegal data byte\n", arguments.installer_file, line_num);
						return 5;
					} else {
						address = address > 0x1000 ? address - 0x1000 : address;
						buffer[address + HEADER_LENGTH + i] = data;
					}
				}
			} else if (type == 1) {
				eof = true;
				printf("EOF\n");
			} else {
				printf("UNKNOWN type, line %d\n", line_num);
			}
			line_num++;
		}
		if (!eof)
			printf("Something was wrong: EOF not found\n");

		fclose(inptr);
	}


	/*
	 * Fill in the header with the magic, file-length, header sum, payload sum
	 * and the reserved bytes
	 */ 
	*(uint32_t *)(buffer + 0x00) = htonl(arguments.magic);
	*(uint32_t *)(buffer + 0x04) = htonl(filesize - HEADER_LENGTH);
	uint32_t sum = 0;
//	for (int i = HEADER_LENGTH; i < filesize; i++)
//		sum += buffer[i];

	for (int i = HEADER_LENGTH; i < SEG_01002_LENGTH; i++)
		sum += buffer[i];
	printf("Payload sum 1 is: 0x%x\n", sum);

	for (int i = HEADER_LENGTH + SEG_01002_LENGTH; i < SEG_01002_LENGTH + SEG_1C000_LENGTH + HEADER_LENGTH; i++)
		sum += buffer[i];
	printf("Payload sum 2 is: 0x%x\n", sum);

	sum += 0xff * HEADER_LENGTH;
	printf("Payload sum with header is: 0x%x\n", sum);

	for (int i = 2 * HEADER_LENGTH + SEG_01002_LENGTH + SEG_1C000_LENGTH; i < filesize; i++)
		sum += buffer[i];
	printf("Payload sum is: 0x%x\n", sum);
	*(uint32_t *)(buffer + 0x0c) = htonl(sum);

	*(uint32_t *)(buffer + 0x10) = htonl(arguments.reserved);

	sum = 0;
	for (int i = 0; i < HEADER_LENGTH; i++)
		sum += buffer[i];
	printf("Header checksum is: 0x%x\n", sum);
	*(uint32_t *)(buffer + 0x08) = htonl(sum);

	// Second header is copy of initial one:
	*(uint32_t *)(buffer + 0x00 + HEADER_LENGTH + SEG_1d000_OFFSET) = *(uint32_t *)(buffer + 0x00);
	*(uint32_t *)(buffer + 0x04 + HEADER_LENGTH + SEG_1d000_OFFSET) = *(uint32_t *)(buffer + 0x04);
	*(uint32_t *)(buffer + 0x08 + HEADER_LENGTH + SEG_1d000_OFFSET) = *(uint32_t *)(buffer + 0x08);
	*(uint32_t *)(buffer + 0x0c + HEADER_LENGTH + SEG_1d000_OFFSET) = *(uint32_t *)(buffer + 0x0c);
	*(uint32_t *)(buffer + 0x10 + HEADER_LENGTH + SEG_1d000_OFFSET) = *(uint32_t *)(buffer + 0x10);

	if (filesize) {
		if (arguments.output_file)
			outptr = creat(arguments.output_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		else
			outptr = mkstemp(tmpfilename);

		if (!outptr) {
			printf("Cannot open %s\n", arguments.output_file ? arguments.output_file : tmpfilename);
			return 5;
		}
		size_t written = write(outptr, buffer, filesize);

		if (written != filesize) {
			printf("Error writing output file.\n");
			return 5;
		}
		close(outptr);

		if (!arguments.output_file)
			rename(tmpfilename, argv[arg_index]);
	}

	return 0;
}

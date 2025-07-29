/*
 * Adds data files into specified locations of an image, optionally creates
 * an index in the form of a header file
 */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <stdbool.h>

#define OFFSET 2

// Use a 4MB buffer, the same as the flash rom size
#define BUFFER_SIZE 0x400000
unsigned char buffer[BUFFER_SIZE];
FILE *inptr, *dataptr;
int outptr;

const char *argp_program_version = "fileadder 0.1";
const char *argp_program_bug_address = "<git@logicog.de>";
static char doc[] = "Adds a file or a directory of files into an image";
static char args_doc[] = "addfile [options] INPUT_IMAGE";
static struct argp_option options[] = {
    { "size", 's', "SIZE", OPTION_ARG_OPTIONAL, "Resize image"},
    { "output", 'o', "FILE", 0, "Output image file name instead of overwriting input image"},
    { "data", 'd', "FILE", 0, "File or directory to add to image"},
    { "address", 'a', 0, OPTION_ARG_OPTIONAL, "Address where data is placed, default is 0x1000000 if option is used, otherwise 0x1fd000"},
    { 0 }
};


struct arguments {
	int size, address;
	char *output_file;
	char *data_file;
	bool overwrite;
};


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
	switch (key) {
	case 'a':
		arguments->address = arg? atoi(arg): 0x100000; // Default size is 2MB
		break;
	case 's':
		arguments->size = arg? atoi(arg): 0x200000; // Default size is 2MB
		break;
	case 'o':
		arguments->output_file = arg;
		arguments->overwrite = false;
		break;
	case 'd':
		arguments->data_file = arg;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

int main(int argc, char **argv)
{
	char * line = NULL;
	size_t len = 0;
	struct arguments arguments;
	int arg_index;
	char tmpfilename[] = "image_XXXXXX";

	arguments.address = 0x1fd000;
	arguments.size = 0;
	arguments.overwrite = true;

	argp_parse(&argp, argc, argv, 0, &arg_index, &arguments);
	if (!arg_index)
		argp_usage (0);

	memset(buffer, BUFFER_SIZE, 1);
	inptr = fopen(argv[arg_index], "rb");
	if (inptr == NULL) {
		printf("Cannot open %s\n", argv[1]);
		return 5;
	}

	fseek(inptr, 0L, SEEK_END);
	size_t filesize = ftell(inptr);
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

	// Now that the beginning of the buffer is filled with out image, optionally resize the image
	filesize = arguments.size? arguments.size : filesize;

	dataptr = fopen(arguments.data_file, "r");
	if (dataptr == NULL) {
		printf("Cannot open %s\n", arguments.data_file);
		return 5;
	}
	size_t data_read = fread(&buffer[arguments.address], 1, sizeof(buffer) - arguments.address, inptr);
	fclose(dataptr);

	if (data_read)
		printf("Data inserted at 0x%x, size: %ld\n", arguments.address, data_read);

	if (!arguments.overwrite)
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

	if (arguments.overwrite)
		rename(tmpfilename, argv[arg_index]);

	return 0;
}

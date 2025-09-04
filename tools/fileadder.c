/*
 * Adds data files into specified locations of an image, optionally creates
 * an index in the form of a header file
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <stdbool.h>

#define OFFSET 2

// Use a 4MB buffer, the same as the flash rom size
#define BUFFER_SIZE 0x400000
char buffer[BUFFER_SIZE];
FILE *inptr, *dataptr, *ofile;
int outptr;
#define PATH_SIZE 20480
#define INDEX_SIZE 20480
#define DEF_SIZE 20480
char pathbuffer[PATH_SIZE];
char ibuf[INDEX_SIZE];
char dbuf[DEF_SIZE];
char fbuf[DEF_SIZE];
char xbuf[DEF_SIZE];
int defbuf_p;
int ibuf_p;
int fbuf_p;
int xbuf_p;
bool addsDir = false;
int callNum = 0;

const char *argp_program_version = "fileadder 0.1";
const char *argp_program_bug_address = "<git@logicog.de>";
static char doc[] = "Adds a file or a directory of files into an image";
static char args_doc[] = "addfile [options] INPUT_IMAGE";
static struct argp_option options[] = {
    { "size", 's', "SIZE", 0, "Resize image"},
    { "output", 'o', "FILE", 0, "Output image file name instead of overwriting input image"},
    { "data", 'd', "FILE", 0, "File or directory to add to image"},
    { "address", 'a', "SIZE", 0, "Address where data is placed, default is 0x1000000 if option is used, otherwise 0x1fd000"},
    { "prefix", 'p', "FILE", 0, "Prefix for header and index file generation"},
    { "bank", 'b', "BANKNAME", 0, "Generate #pragma with given bank-name"},
    { 0 }
};


struct arguments {
	int size, address;
	char *output_file;
	char *data_file;
	char *prefix;
	char *bank;
	bool overwrite;
	bool add_zero;
};


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
	switch (key) {
	case 'a':
		arguments->address = arg? atoi(arg): 0x100000; // Default address
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
	case 'p':
		arguments->prefix = arg;
		break;
	case 'n':
		arguments->add_zero = false;
		break;
	case 'b':
		arguments->bank = arg? arg : "BANK1";
		break;
	default:
		return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


static struct argp argp = {
	options, parse_opt, args_doc, doc, 0, 0, 0
};


int addfile(const char *name, int addr)
{
	int dataptr = open(name, 0);
	if (!dataptr) {
		fprintf(stderr, "%s: ", name);
		perror("Cannot open file for reading");
		return 0;
	}

	int data_read = read(dataptr, &buffer[addr], sizeof(buffer) - addr);
	if (data_read < 0) {
		perror("Error reading file");
		return 0;
	}
	close(dataptr);

	return data_read;
}


int hasSuffix(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);

    if (lensuffix >  lenstr)
        return 0;

    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}


char *getMime(const char *name)
{
	if (hasSuffix(name, ".html"))
		return "mime_HTML";
	else if (hasSuffix(name, ".svg"))
		return "mime_SVG";
	else if (hasSuffix(name, ".ico"))
		return "mime_SVG";
	else if (hasSuffix(name, ".png"))
		return "mime_PNG";
	else if (hasSuffix(name, ".js"))
		return "mime_JS";
	else if (hasSuffix(name, ".css"))
		return "mime_CSS";
	return "mime_TXT";
}


int addidx(const char *name, int addr, int len)
{
	char s[256];
	int i = 0;

	while (name[i]) {
		s[i] = name[i] == '.'? '_' : name[i];
		i++;
	}
	s[i] = '\0';

	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p, "#define FDATA_START_%s 0x%x\n", s, addr);
	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p, "#define FDATA_SIZE_%s %d\n", s, len);
	ibuf_p += snprintf(&ibuf[ibuf_p], INDEX_SIZE - ibuf_p, "  {\"/%s\", FDATA_START_%s, FDATA_SIZE_%s, %s},\n", name, s, s, getMime(name));
	if (!strcmp(name, "index.html"))
		ibuf_p += snprintf(&ibuf[ibuf_p], INDEX_SIZE - ibuf_p, "  {\"/\", FDATA_START_%s, FDATA_SIZE_%s, mime_HTML},\n", s, s);
	return 0;
}

/*
 * Replaces calls of the type #{function} in .html files being added
 * It is assumed that the possibly expanded string will fit into the buffer
 * Returns the new length of the string
 */
int replaceCalls(int pos)
{
	int i = 0;
	char function_buf[256];

	while (buffer[pos + i]) {
		if ((buffer[pos + i]) == '#' && (buffer[pos + i + 1] == '{')) {
			int j = 2;
			while (buffer[pos + i + j]) {
				if (buffer[pos + i + j] == '}')
					break;
				function_buf[j - 2] = buffer[pos + i + j];
				j++;
				if (j >= 255) {
					function_buf[255] = '\0';
					fprintf(stderr, "Function not terminated %s ", function_buf);
					exit(5);
				}

			}
			function_buf[j - 2] = '\0';
			if (!buffer[pos + i + j])
				return i + j;
			// Because the buffers overlap we cannot use strcpy
			memmove(&buffer[pos + i + 5], &buffer[pos + i + j], strlen(&buffer[pos + i + j]) + 1);
			sprintf(&buffer[pos + i + 2], "%03d", callNum);
			buffer[pos + i + 5] = '}';
			i += 6;
			fbuf_p += snprintf(&fbuf[fbuf_p], DEF_SIZE - fbuf_p, "  %s,\n", function_buf);
			xbuf_p += snprintf(&xbuf[xbuf_p], DEF_SIZE - xbuf_p, "extern uint16_t %s(void);\n", function_buf);
			callNum++;
		}
		i++;
	}

	return strlen(&buffer[pos]);
}


int main(int argc, char **argv)
{
	char * line = NULL;
	size_t len = 0;
	struct arguments arguments;
	int arg_index;
	char tmpfilename[] = "image_XXXXXX";
	struct stat s;

	arguments.add_zero = true;
	arguments.address = 0x1fd000;
	arguments.size = 0;
	arguments.overwrite = true;
	arguments.prefix = 0;
	arguments.bank = NULL;

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

	// Add preambles to index and header file
	defbuf_p = ibuf_p = xbuf_p = fbuf_p = 0;
	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p, "// This file is automatically generated, do not edit!\n\n");
	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p, "#ifndef FDATA_DEFS_H\n");
	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p, "#define FDATA_DEFS_H\n\n");
	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p, "#include <stdint.h>\n\n");
	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p,
			     "typedef enum mime_type_e {\n  mime_HTML = 0,\n  mime_SVG,\n  mime_ICO,\n  mime_PNG,\n  mime_JS,\n  mime_CSS,\n  mime_TXT\n} mime_type_t;\n\n");
	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p, "struct f_data {\n  __code char *file;\n  uint32_t start;\n  uint16_t len;\n  mime_type_t mime;\n};\n\n");
	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p, "typedef uint16_t (* fcall_ptr)(void);\n\n");

	ibuf_p += snprintf(&ibuf[ibuf_p], INDEX_SIZE - ibuf_p, "// This file is automatically generated, do not edit!\n\n");
	if (arguments.prefix)
		ibuf_p += snprintf(&ibuf[ibuf_p], INDEX_SIZE - ibuf_p, "#include \"%s.h\"\n\n", arguments.prefix);
	if (arguments.bank)
		ibuf_p += snprintf(&ibuf[ibuf_p], INDEX_SIZE - ibuf_p, "#pragma codeseg %s\n\n", arguments.bank);
	ibuf_p += snprintf(&ibuf[ibuf_p], INDEX_SIZE - ibuf_p, " __code char * __code mime_strings[] = {\n  \"text/html\",\n  \"image/svg+xml\",\n"
		"  \"image/svg+xml\",\n  \"image/png\",\n  \"text/javascript\",\n  \"text/css\",\n  \"text/plain\"};\n\n");
	ibuf_p += snprintf(&ibuf[ibuf_p], INDEX_SIZE - ibuf_p, "__code struct f_data f_data[] = {\n");
	fbuf_p += snprintf(&fbuf[fbuf_p], DEF_SIZE - fbuf_p, "\n__code fcall_ptr f_calls[] = {\n");

	// Now that the beginning of the buffer is filled with out image, optionally resize the image
	if (filesize)
		filesize = arguments.size? arguments.size : filesize;

	if (stat(arguments.data_file, &s) == 0 && s.st_mode & S_IFDIR) {
		printf("Adding entries in directory %s\n", arguments.data_file);
		addsDir = true;
		DIR *dirptr = opendir(arguments.data_file);
		if (!dirptr) {
			fprintf(stderr, "%s: ", arguments.data_file);
			perror("Error opening directory");
			return 5;
		}
		struct dirent *in_file;
		int addr = arguments.address;
		while ( (in_file = readdir(dirptr)) )  {
			snprintf(pathbuffer, PATH_SIZE, "%s/%s", arguments.data_file, in_file->d_name);
			if (stat(pathbuffer, &s) < 0) {
				fprintf(stderr, "%s: ", pathbuffer);
				perror("Stat failed");
				continue;
			}
			if (! (s.st_mode & S_IFREG))
				continue;

			size_t data_read = addfile(pathbuffer, addr);
			if (data_read) {
				if (arguments.add_zero) {
					buffer[addr + data_read + 1] = '\0';
					data_read++;
				}
				printf("Data inserted from %s at 0x%x, size: %ld\n", pathbuffer, addr, data_read);
			}
			int old_len = data_read;
			data_read = replaceCalls(addr);
			if (old_len > data_read)
				memset(buffer + addr + data_read, 0, old_len - data_read);
			addidx(in_file->d_name, addr, data_read);
			addr += data_read;
		}
	} else {
		size_t data_read = addfile(arguments.data_file, arguments.address);

		if (data_read) {
			if (arguments.add_zero) {
				buffer[arguments.address + data_read + 1] = '\0';
				data_read++;
			}
			printf("Data inserted at 0x%x, size: %ld\n", arguments.address, data_read);
		}
	}

	ibuf_p += snprintf(&ibuf[ibuf_p], INDEX_SIZE - ibuf_p, "  {0, 0, 0}\n};\n");
	fbuf_p += snprintf(&fbuf[fbuf_p], DEF_SIZE - fbuf_p, "};\n");
	defbuf_p += snprintf(&dbuf[defbuf_p], DEF_SIZE - defbuf_p, "#endif\n");

	if (filesize) {
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
	}

	if (arguments.prefix) {
		snprintf(pathbuffer, PATH_SIZE, "%s.%s", arguments.prefix, "h");
		ofile = fopen(pathbuffer, "w");
		if (!ofile) {
			fprintf(stderr, "%s: ", pathbuffer);
			perror("Open failed");
			return 5;
		}
		fputs(dbuf, ofile);
		fclose(ofile);

		snprintf(pathbuffer, PATH_SIZE, "%s.%s", arguments.prefix, "c");
		ofile = fopen(pathbuffer, "w");
		if (!ofile) {
			fprintf(stderr, "%s: ", pathbuffer);
			perror("Open failed");
			return 5;
		}
		fputs(ibuf, ofile);
		fputs(xbuf, ofile);
		fputs(fbuf, ofile);
		fclose(ofile);
	} else if (addsDir){
		puts(dbuf);
		puts(xbuf);
		puts(ibuf);
		puts(fbuf);
	}
	return 0;
}

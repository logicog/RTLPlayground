/*
 * injector <input.bin> <patch.hex> <output.bin>
 * e.g.:  ./injector kp-9000-6hx-x.bin RTLPlayground/rwintercept.hex kp-9000-6hx-x_patched.bin
 */
#include <stdio.h>
#include <stdlib.h>

#define OFFSET 2

// Use a 4MB buffer, the same as the flash rom size
#define BUFFER_SIZE 0x400000
unsigned char buffer[BUFFER_SIZE];
FILE *inptr, *outptr, *patchptr;

int main(int argc, char **argv)
{
	char * line = NULL;
	size_t len = 0;
	unsigned int addr, value;

	if (argc != 4) {
		printf("I only work with 3 arguments\n");
		return 5;
	}

	inptr = fopen(argv[1], "rb");
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

	printf("Bytes read:: %ld\n", bytes_read);
	
	if (bytes_read != filesize) {
		printf("Error reading input file.\n");
		return 5;
	}
	fclose(inptr);

	patchptr = fopen(argv[2], "r");
	if (patchptr == NULL) {
		printf("Cannot open %s\n", argv[1]); 
		return 5;
	}

	size_t l;
	int patches = 0;
	while ((l = getline(&line, &len, patchptr)) != -1) {
//		printf("Retrieved line of length %zu:\n", l);
		sscanf(line, "%x: %x", &addr, &value);
		if (addr < 0x4000)
			addr += OFFSET;
		if (addr < filesize) {
			patches++;
			buffer[addr] = value;
//			printf("%s gives %x <- %x\n", line,addr, value);
		} else {
			printf("Cannot patch: %s\n", line);
		}
	}
	printf("Patched %d bytes.\n", patches);
	fclose(patchptr);
	if (line)
		free(line);

	outptr = fopen(argv[3], "wb");
	if (outptr == NULL) {
		printf("Cannot open %s\n", argv[1]); 
		return 5;
	}
	size_t written = fwrite(buffer, 1, filesize, outptr);
	
	if (written != filesize) {
		printf("Error writing output file.\n");
		return 5;
	}
	fclose(outptr);

	return 0;
}

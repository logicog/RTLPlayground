#ifndef BLAKE3_H
#define BLAKE3_H

#include <stdint.h>

// Single final block, unkeyed BLAKE3 with a 32-byte output. SDCC MCS-51 is little-endian.
struct blake3_work {
	uint8_t block[32];
	uint32_t state[16];
};

// Hash at most 32 input bytes and write the digest back to the same block.
void blake3_digest(__xdata struct blake3_work *work, uint8_t len) __reentrant;

#endif

#ifndef _CHACHA_H_
#define _CHACHA_H_

#include <stdint.h>

struct chacha20_t {
	void *block;		// 64 bytes written here
        uint8_t key[32];     	// 256-bit secret key
        uint8_t nonce[12];   	// 96-bit nonce
        uint16_t cnt;		// 16-bit block counter 1, 2..
};


// perform the permutation for "dr" doublerounds
void chacha_perm(__xdata uint8_t st[64], uint8_t dr);

// generate a block of ChaCha20 keystream as per RFC7539
void chacha20_block(void);

#endif

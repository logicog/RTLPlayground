#ifndef _CHACHA_H_
#define _CHACHA_H_

#include <stdint.h>

struct chacha20_t {
	uint8_t wstate[64];		// 64 bytes written here
	uint8_t constant[16];		// 128 bit constant
        uint8_t key[32];		// 256-bit secret key
        uint32_t cnt;			// 32-bit block counter 1, 2.. Big Endian!
        uint8_t nonce[12];		// 96-bit nonce
        __xdata uint8_t *plaintext;
        uint16_t length;
        __xdata uint8_t *cyphertext;
};

// Encrypt a plaintext with ChaCha20 as per RFC7539
void chacha20_encrypt(void);

// Test ChaCha20 using the example from RFC7539
void chacha20_test(void);

#endif

#ifndef _CHACHA_H_
#define _CHACHA_H_

#include <stdint.h>

// In order to test, CHACHA20_TEST needs to be defined
// #define CHACHA20_TEST

struct chacha20_t {
	uint8_t wstate[64];		// 64 bytes written here
	uint8_t constant[16];		// 128 bit constant (4 Big-Endian u32)
        uint8_t key[32];		// 256-bit secret key (8 Big-Endian u32)
        uint8_t cnt[4];			// 32-bit block counter 1, 2.. Big Endian!
        uint8_t nonce[12];		// 96-bit nonce (3 Big-Endian u32)
        __xdata uint8_t *plaintext;
        uint16_t length;
        __xdata uint8_t *cyphertext;
};

// Encrypt a plaintext with ChaCha20 as per RFC7539
void chacha20_encrypt(void) __banked;

#ifdef CHACHA20_TEST
// Test ChaCha20 using the example from RFC7539
void chacha20_test(void) __banked;
#endif

#endif

#ifndef _AES128_H_
#define _AES128_H_

#include <stdint.h>

#define AES128_BLOCK_LEN	16
#define AES128_KEY_LEN		16

void aes128_encrypt(__xdata const uint8_t * __xdata key, __xdata uint8_t * __xdata block) __banked;

#endif

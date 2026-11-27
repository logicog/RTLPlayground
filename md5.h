#ifndef _MD5_H_
#define _MD5_H_

#include <stdint.h>

#define MD5_DIGEST_LENGTH 16
#define MD5_BLOCK_LENGTH 64

struct md5_ctx {
    uint32_t state[4];		/* A, B, C, D */
    uint32_t length;		/* bytes hashed so far */
    uint8_t buf[MD5_BLOCK_LENGTH];	/* pending partial block */
    uint8_t n;			/* bytes in buf */
};

void md5_init(__xdata struct md5_ctx *c) __banked __reentrant;
void md5_update(__xdata struct md5_ctx *c, __xdata const uint8_t *p, uint16_t len) __banked __reentrant;
void md5_final(__xdata struct md5_ctx *c, __xdata uint8_t *digest) __banked __reentrant;
void hmac_md5(__xdata const uint8_t *key, uint8_t key_len, __xdata const uint8_t *msg, uint16_t msg_len, __xdata uint8_t *digest) __banked __reentrant;

#endif

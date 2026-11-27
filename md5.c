#pragma codeseg BANK3
#pragma constseg BANK3

#include <stdint.h>
#include "rtl837x_common.h"
#include "md5.h"

static __code const uint32_t md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static __code const uint8_t md5_s[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static __xdata uint32_t md5_m[16];
static __xdata uint32_t md5_a, md5_b, md5_c, md5_d, md5_f;
static __xdata uint8_t md5_i, md5_g;
static __xdata struct md5_ctx hmac_ctx;
static __xdata uint8_t hmac_pad[MD5_BLOCK_LENGTH];
static __xdata uint8_t hmac_inner[MD5_DIGEST_LENGTH];

static void md5_block(__xdata struct md5_ctx *c, __xdata const uint8_t *p) __reentrant
{
    for (md5_i = 0; md5_i < 16; md5_i++) {
        md5_m[md5_i] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        p += 4;
    }
    md5_a = c->state[0];
    md5_b = c->state[1];
    md5_c = c->state[2];
    md5_d = c->state[3];
    for (md5_i = 0; md5_i < 64; md5_i++) {
        switch (md5_i >> 4) {
        case 0:
            md5_f = (md5_b & md5_c) | (~md5_b & md5_d);
            md5_g = md5_i;
            break;
        case 1:
            md5_f = (md5_d & md5_b) | (~md5_d & md5_c);
            md5_g = (5 * md5_i + 1) & 15;
            break;
        case 2:
            md5_f = md5_b ^ md5_c ^ md5_d;
            md5_g = (3 * md5_i + 5) & 15;
            break;
        default:
            md5_f = md5_c ^ (md5_b | ~md5_d);
            md5_g = (7 * md5_i) & 15;
            break;
        }
        md5_f += md5_a + md5_k[md5_i] + md5_m[md5_g];
        md5_a = md5_d;
        md5_d = md5_c;
        md5_c = md5_b;
        md5_b += (md5_f << md5_s[md5_i]) | (md5_f >> (32 - md5_s[md5_i]));
    }
    c->state[0] += md5_a;
    c->state[1] += md5_b;
    c->state[2] += md5_c;
    c->state[3] += md5_d;
}

void md5_init(__xdata struct md5_ctx *c) __banked __reentrant
{
    c->state[0] = 0x67452301;
    c->state[1] = 0xefcdab89;
    c->state[2] = 0x98badcfe;
    c->state[3] = 0x10325476;
    c->length = 0;
    c->n = 0;
}

void md5_update(__xdata struct md5_ctx *c, __xdata const uint8_t *p, uint16_t len) __banked __reentrant
{
    c->length += len;
    while (len) {
        c->buf[c->n++] = *p++;
        len--;
        if (c->n == MD5_BLOCK_LENGTH) {
            md5_block(c, c->buf);
            c->n = 0;
        }
    }
}

void md5_final(__xdata struct md5_ctx *c, __xdata uint8_t *digest) __banked __reentrant
{
    uint32_t bits = c->length << 3;
    uint8_t i;

    c->buf[c->n++] = 0x80;
    while (c->n != 56) {
        if (c->n == MD5_BLOCK_LENGTH) {
            md5_block(c, c->buf);
            c->n = 0;
        }
        c->buf[c->n++] = 0;
    }
    for (i = 0; i < 8; i++) {
        c->buf[56 + i] = i < 4 ? (uint8_t)(bits >> (8 * i)) : 0;
    }
    md5_block(c, c->buf);
    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
        digest[i] = (uint8_t)(c->state[i >> 2] >> (8 * (i & 3)));
}

void hmac_md5(__xdata const uint8_t *key, uint8_t key_len, __xdata const uint8_t *msg, uint16_t msg_len, __xdata uint8_t *digest) __banked __reentrant
{
    uint8_t i;

    for (i = 0; i < MD5_BLOCK_LENGTH; i++)
        hmac_pad[i] = (i < key_len ? key[i] : 0) ^ 0x36;
    md5_init(&hmac_ctx);
    md5_update(&hmac_ctx, hmac_pad, MD5_BLOCK_LENGTH);
    md5_update(&hmac_ctx, msg, msg_len);
    md5_final(&hmac_ctx, hmac_inner);
    for (i = 0; i < MD5_BLOCK_LENGTH; i++)
        hmac_pad[i] ^= 0x36 ^ 0x5c;
    md5_init(&hmac_ctx);
    md5_update(&hmac_ctx, hmac_pad, MD5_BLOCK_LENGTH);
    md5_update(&hmac_ctx, hmac_inner, MD5_DIGEST_LENGTH);
    md5_final(&hmac_ctx, digest);
}

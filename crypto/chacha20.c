#include "rtl837x_common.h"
#include "chacha.h"

// generate a block of ChaCha20 keystream as per RFC7539


__xdata struct chacha20_t __at(0x7000) chacha20;


void chacha20_block(void)
{
    __code uint32_t fixed[4] = { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 };

    memcpyc(chacha20.block, fixed, 16);
    memcpy(chacha20.block + 16, chacha20.key, 32);
    memcpy(chacha20.block + 48, &chacha20.cnt, 4);
    memcpy(chacha20.block + 52, chacha20.nonce, 12);

    chacha_perm(chacha20.block, 10);             // 10 double-rounds

    for (uint8_t i = 0; i < 4; i++)
        ((uint32_t *) chacha20.block)[i] += fixed[i];
    for (uint8_t i = 0; i < 8; i++)
        ((uint32_t *) chacha20.block)[i + 4] += ((const uint32_t *) chacha20.key)[i];
    ((uint32_t *) chacha20.block)[12] += chacha20.cnt;
    for (uint8_t i = 0; i < 3; i++)
        ((uint32_t *) chacha20.block)[i + 13] += ((const uint32_t *) chacha20.nonce)[i];
}

#include "chacha.h"

#define ROTL32(x, y)  (((x) << (y)) ^ ((x) >> (32 - (y))))

// ChaCha Quarter Round unrolled as a macro

#define CHACHA_QR(A, B, C, D) { \
    A += B; D ^= A; D = ROTL32(D, 16);  \
    C += D; B ^= C; B = ROTL32(B, 12);  \
    A += B; D ^= A; D = ROTL32(D, 8);   \
    C += D; B ^= C; B = ROTL32(B, 7);   \
}

// ChaCha permutation -- dr is the number of double rounds

void chacha_perm(__xdata uint8_t st[64], uint8_t dr)
{
    __xdata uint32_t *v = (__xdata uint32_t *) st;

    while (dr) {
        CHACHA_QR( v[ 0], v[ 4], v[ 8], v[12] );
        CHACHA_QR( v[ 1], v[ 5], v[ 9], v[13] );
        CHACHA_QR( v[ 2], v[ 6], v[10], v[14] );
        CHACHA_QR( v[ 3], v[ 7], v[11], v[15] );
        CHACHA_QR( v[ 0], v[ 5], v[10], v[15] );
        CHACHA_QR( v[ 1], v[ 6], v[11], v[12] );
        CHACHA_QR( v[ 2], v[ 7], v[ 8], v[13] );
        CHACHA_QR( v[ 3], v[ 4], v[ 9], v[14] );
	dr--;
    }
}

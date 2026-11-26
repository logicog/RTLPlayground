// Each input starts with a four-byte salt. The second password is the 20-byte maximum.
// The expected digests are fixed so the test needs neither tools/passwd nor b3sum.
// The first 16 bytes are the digest printed by tools/passwd blake3 42 <salt>.
#include "crypto/blake3.h"

static const __code uint8_t short_input[] = "\x77\x86\x07\xdc"
					    "rtlplayground";
static const __code uint8_t max_input[] = "\xb2\xd4\x72\x9d"
					  "01234567890123456789";
static __xdata struct blake3_work work;
static volatile __xdata uint8_t __at(0xffff) simif;

static void hash_vector(const __code uint8_t *input, uint8_t len)
{
	for (uint8_t i = 0; i < len; i++) {
		work.block[i] = input[i];
	}
	for (uint8_t n = 0; n < 42; n++) {
		blake3_digest(&work, n ? 32 : len);
	}
	for (uint8_t i = 0; i < 32; i++) {
		simif = 'w';
		simif = work.block[i];
	}
}

void main(void)
{
	hash_vector(short_input, sizeof(short_input) - 1);
	hash_vector(max_input, sizeof(max_input) - 1);
	simif = 's';
}

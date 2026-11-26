#include <stddef.h>
#include "blake3.h"

#pragma codeseg BANK4
#pragma constseg BANK4

static const __code uint32_t initial_state[16] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0, 0, 0, 1 | 2 | 8}; // CHUNK_START | CHUNK_END | ROOT
// Each byte selects the two message words for one G, first word in the high nibble.
static const __code uint8_t schedule[][8] = {
	{0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef},
	{0x26, 0x3a, 0x70, 0x4d, 0x1b, 0xc5, 0x9e, 0xf8},
	{0x34, 0xac, 0xd2, 0x7e, 0x65, 0x90, 0xbf, 0x81},
	{0xa7, 0xc9, 0xe3, 0xdf, 0x40, 0xb2, 0x58, 0x16},
	{0xcd, 0x9b, 0xfa, 0xe8, 0x72, 0x53, 0x01, 0x64},
	{0x9e, 0xb5, 0x8c, 0xf1, 0xd3, 0x0a, 0x26, 0x47},
	{0xbf, 0x50, 0x19, 0x86, 0xea, 0x2c, 0x34, 0x7d},
};

void blake_regs_round(const __code uint8_t *schedule, __xdata struct blake3_work *work);

// blake_regs.asm uses these offsets from the caller's workspace.
_Static_assert(offsetof(struct blake3_work, block) == 0, "assembly block offset");
_Static_assert(offsetof(struct blake3_work, state) == 32, "assembly state offset");

void blake3_digest(__xdata struct blake3_work *work, uint8_t len) __reentrant
{
	for (uint8_t i = len; i < sizeof(work->block); i++) {
		work->block[i] = 0;
	}
	__xdata uint8_t *state = (__xdata uint8_t *)work->state;
	for (uint8_t i = 0; i < sizeof(work->state); i++) {
		state[i] = ((__code const uint8_t *)initial_state)[i];
	}
	work->state[14] = len;
	for (uint8_t round = 0; round < 7; round++) {
		blake_regs_round(schedule[round], work);
	}
	for (uint8_t i = 0; i < sizeof(work->block); i++) {
		work->block[i] = state[i] ^ state[sizeof(work->block) + i];
	}
}

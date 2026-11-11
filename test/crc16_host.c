/*
 * crc16_host.c - host equivalent of the firmware's crc16_bank1 (crc16.asm).
 *
 * The firmware folds the image a byte at a time into crc_value with a
 * table-driven CRC-16 (reflected, polynomial 0xA001 = CRC-16/ARC, init 0);
 * a build appends two bytes so the CRC over the whole image is 0xB001. This
 * is the bitwise equivalent, and test_update_apply checks it reproduces
 * 0xB001 on a real built image.
 */
#include <stdint.h>

uint16_t crc_value;

static uint16_t step(uint16_t crc, uint8_t b)
{
	crc ^= b;
	for (int k = 0; k < 8; k++)
		crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
	return crc;
}

void crc16_bank1(uint8_t *v)
{
	crc_value = step(crc_value, *v);
}

/* Helpers for tests (not part of the firmware interface). */
uint16_t crc16_host_step(uint16_t crc, uint8_t b) { return step(crc, b); }

uint16_t crc16_host_buf(const uint8_t *d, uint32_t n)
{
	uint16_t crc = 0;
	for (uint32_t i = 0; i < n; i++)
		crc = step(crc, d[i]);
	return crc;
}

/*
 * test_md5.c - md5.c against the RFC 1321 and RFC 2202 vectors.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "md5.h"
#include "support.h"

static struct md5_ctx ctx;
static uint8_t digest[16];
static char hex[33];

static const char *md5_hex(const char *msg)
{
	md5_init(&ctx);
	md5_update(&ctx, (const uint8_t *)msg, strlen(msg));
	md5_final(&ctx, digest);
	for (int i = 0; i < 16; i++) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
	return hex;
}

static const char *hmac_hex(const uint8_t *key, uint8_t klen, const char *msg)
{
	hmac_md5(key, klen, (const uint8_t *)msg, strlen(msg), digest);
	for (int i = 0; i < 16; i++) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
	return hex;
}

int main(void)
{
	uint8_t k0b[16], kaa[16];
	char big[1001];
	int i;

	printf("== md5.c tests ==\n");
	CHECK(strcmp(md5_hex(""), "d41d8cd98f00b204e9800998ecf8427e") == 0, "md5 of the empty string");
	CHECK(strcmp(md5_hex("abc"), "900150983cd24fb0d6963f7d28e17f72") == 0, "md5 of abc");
	CHECK(strcmp(md5_hex("message digest"), "f96b697d7cb7938d525a2f31aaf161d0") == 0, "md5 of message digest");
	CHECK(strcmp(md5_hex("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"), "d174ab98d277d9f5a5611c2c9f419d9f") == 0, "md5 of 62 alphanumerics");
	CHECK(strcmp(md5_hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890"), "57edf4a22be3c955ac49da2e2107b67a") == 0, "md5 of 80 digits, two blocks");
	memset(big, 'a', 1000); big[1000] = 0;
	CHECK(strcmp(md5_hex(big), "cabe45dcc9ae5b66ba86600cca6b8ba8") == 0, "md5 of 1000 a, many blocks");
	md5_init(&ctx);
	for (i = 0; i < 1000; i++) md5_update(&ctx, (const uint8_t *)"a", 1);
	md5_final(&ctx, digest);
	for (i = 0; i < 16; i++) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
	CHECK(strcmp(hex, "cabe45dcc9ae5b66ba86600cca6b8ba8") == 0, "byte-at-a-time update gives the same digest");

	memset(k0b, 0x0b, 16);
	memset(kaa, 0xaa, 16);
	CHECK(strcmp(hmac_hex(k0b, 16, "Hi There"), "9294727a3638bb1c13f48ef8158bfc9d") == 0, "hmac-md5 RFC 2202 case 1");
	CHECK(strcmp(hmac_hex((const uint8_t *)"Jefe", 4, "what do ya want for nothing?"), "750c783e6ab0b503eaa86e310a5db738") == 0, "hmac-md5 RFC 2202 case 2");
	memset(big, 0xdd, 50); big[50] = 0;
	hmac_md5(kaa, 16, (const uint8_t *)big, 50, digest);
	for (i = 0; i < 16; i++) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
	CHECK(strcmp(hex, "56be34521d144c88dbb8c733f0e8b3f6") == 0, "hmac-md5 RFC 2202 case 3");
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

/*
 * TOTP (RFC 6238) with HMAC-SHA1, 6 digits, 30s step, +/-1 step window.
 * Wall-clock time comes from the SNTP client; verification fails closed
 * while time is not synchronized.
 *
 * The SHA-1 here is a fixed two-block variant: HMAC over an 8-byte
 * counter with a key of at most 64 bytes needs exactly two blocks per
 * hash (pad block + short message block), so no generic streaming or
 * padding logic is carried around.
 */
#include "machine.h"
#include "totp.h"
#include "sntp.h"
#include "rtl837x_common.h"
#include "cmd_parser.h"

#pragma codeseg BANK3
#pragma constseg BANK3

__xdata uint8_t totp_enabled;
__xdata uint8_t totp_keylen;
static __xdata uint8_t totp_key[TOTP_KEY_MAX];

static __xdata uint32_t H[5];
static __xdata uint32_t W[16];
static __xdata uint8_t block[64];
static __xdata uint8_t digest[20];

/* 32-bit rotates via byte operations on rot_r: `(v << n) | (v >> (32 - n))`
 * makes SDCC spill two 4-byte temporaries into internal RAM, which has no
 * room left. SDCC mcs51 stores integers little-endian, so p[0] is the LSB. */
static __xdata uint32_t rot_r;
#define ROT_P ((__xdata uint8_t *)&rot_r)


static void rotl8_r(void) __reentrant
{
	uint8_t t = ROT_P[3];

	ROT_P[3] = ROT_P[2];
	ROT_P[2] = ROT_P[1];
	ROT_P[1] = ROT_P[0];
	ROT_P[0] = t;
}


static void rotl1_r(void) __reentrant
{
	uint8_t c = ROT_P[3] >> 7;

	ROT_P[3] = (ROT_P[3] << 1) | (ROT_P[2] >> 7);
	ROT_P[2] = (ROT_P[2] << 1) | (ROT_P[1] >> 7);
	ROT_P[1] = (ROT_P[1] << 1) | (ROT_P[0] >> 7);
	ROT_P[0] = (ROT_P[0] << 1) | c;
}


static void rotr1_r(void) __reentrant
{
	uint8_t c = ROT_P[0] << 7;

	ROT_P[0] = (ROT_P[0] >> 1) | (ROT_P[1] << 7);
	ROT_P[1] = (ROT_P[1] >> 1) | (ROT_P[2] << 7);
	ROT_P[2] = (ROT_P[2] >> 1) | (ROT_P[3] << 7);
	ROT_P[3] = (ROT_P[3] >> 1) | c;
}


static void sha1_start(void) __reentrant
{
	H[0] = 0x67452301UL;
	H[1] = 0xefcdab89UL;
	H[2] = 0x98badcfeUL;
	H[3] = 0x10325476UL;
	H[4] = 0xc3d2e1f0UL;
}


/* 8 uint32 locals would strain the 8051's tiny internal RAM; xdata is
 * slower but safe, and a login-time hash has no speed requirement. */
static __xdata uint32_t a, b, c, d, e, f, k, tmp;

static void sha1_block(void) __reentrant
{
	uint8_t i, j;

	j = 0;
	for (i = 0; i < 16; i++) {
		__xdata uint8_t *wp = (__xdata uint8_t *)&W[i];
		wp[3] = block[j++];
		wp[2] = block[j++];
		wp[1] = block[j++];
		wp[0] = block[j++];
	}

	a = H[0]; b = H[1]; c = H[2]; d = H[3]; e = H[4];

	/* Steps kept deliberately small: complex expressions over xdata
	 * operands make SDCC spill temporaries into internal RAM, which is
	 * already nearly full. */
	for (i = 0; i < 80; i++) {
		j = i & 15;
		if (i >= 16) {
			rot_r = W[(i + 13) & 15];
			rot_r ^= W[(i + 8) & 15];
			rot_r ^= W[(i + 2) & 15];
			rot_r ^= W[j];
			rotl1_r();
			W[j] = rot_r;
		}
		if (i < 20) {
			f = b;
			f &= c;
			tmp = ~b;
			tmp &= d;
			f |= tmp;
			k = 0x5a827999UL;
		} else if (i < 40) {
			f = b;
			f ^= c;
			f ^= d;
			k = 0x6ed9eba1UL;
		} else if (i < 60) {
			f = b;
			f &= c;
			tmp = b;
			tmp &= d;
			f |= tmp;
			tmp = c;
			tmp &= d;
			f |= tmp;
			k = 0x8f1bbcdcUL;
		} else {
			f = b;
			f ^= c;
			f ^= d;
			k = 0xca62c1d6UL;
		}
		rot_r = a;	/* rotl 5 = rotl 8, then rotr 3 */
		rotl8_r();
		rotr1_r();
		rotr1_r();
		rotr1_r();
		tmp = rot_r;
		tmp += f;
		tmp += e;
		tmp += k;
		tmp += W[j];
		e = d;
		d = c;
		rot_r = b;	/* rotl 30 = rotr 2 */
		rotr1_r();
		rotr1_r();
		c = rot_r;
		b = a;
		a = tmp;
	}

	H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e;
}


static void sha1_out(void) __reentrant
{
	uint8_t i, j;

	j = 0;
	for (i = 0; i < 5; i++) {
		__xdata uint8_t *hp = (__xdata uint8_t *)&H[i];
		digest[j++] = hp[3];
		digest[j++] = hp[2];
		digest[j++] = hp[1];
		digest[j++] = hp[0];
	}
}


static void block_clear(void)
{
	for (uint8_t i = 0; i < 64; i++)
		block[i] = 0;
}


/* Value passing kept in xdata: uint32 parameters and locals land in the
 * 8051's 256-byte internal RAM, which is already near-full. */
static __xdata uint32_t tc_counter;	/* T for hmac/code computation */
static __xdata uint32_t tc_code;	/* resulting 6-digit code */
static __xdata uint32_t tv_entered;
static __xdata uint32_t tv_t;

/* digest = HMAC-SHA1(totp_key, 8-byte big-endian tc_counter) */
static void hmac_counter(void)
{
	uint8_t i;

	/* inner: SHA1((key ^ ipad) || counter-message) */
	sha1_start();
	block_clear();
	for (i = 0; i < totp_keylen; i++)
		block[i] = totp_key[i];
	for (i = 0; i < 64; i++)
		block[i] ^= 0x36;
	sha1_block();

	block_clear();
	block[4] = ((__xdata uint8_t *)&tc_counter)[3];
	block[5] = ((__xdata uint8_t *)&tc_counter)[2];
	block[6] = ((__xdata uint8_t *)&tc_counter)[1];
	block[7] = ((__xdata uint8_t *)&tc_counter)[0];
	block[8] = 0x80;
	block[62] = 0x02;	/* (64 + 8) * 8 = 576 bits */
	block[63] = 0x40;
	sha1_block();
	sha1_out();

	/* outer: SHA1((key ^ opad) || inner-digest) */
	sha1_start();
	block_clear();
	for (i = 0; i < totp_keylen; i++)
		block[i] = totp_key[i];
	for (i = 0; i < 64; i++)
		block[i] ^= 0x5c;
	sha1_block();

	block_clear();
	for (i = 0; i < 20; i++)
		block[i] = digest[i];
	block[20] = 0x80;
	block[62] = 0x02;	/* (64 + 20) * 8 = 672 bits */
	block[63] = 0xa0;
	sha1_block();
	sha1_out();
}


/* tc_code = 6-digit code for tc_counter */
static void totp_code(void)
{
	uint8_t o;

	hmac_counter();
	o = digest[19] & 0x0f;
	ROT_P[3] = digest[o] & 0x7f;
	ROT_P[2] = digest[o + 1];
	ROT_P[1] = digest[o + 2];
	ROT_P[0] = digest[o + 3];
	tc_code = rot_r % 1000000UL;
}


void totp_init(void) __banked
{
	totp_enabled = 0;
	totp_keylen = 0;
}


uint8_t totp_set_secret(__xdata uint8_t *b32) __banked
{
	uint16_t bits = 0;
	uint8_t nbits = 0;
	uint8_t len = 0;
	uint8_t c;

	while ((c = *b32++)) {
		if (c == '=')
			break;
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		if (c >= 'A' && c <= 'Z')
			c -= 'A';
		else if (c >= '2' && c <= '7')
			c -= '2' - 26;
		else
			return 0;
		bits = (bits << 5) | c;
		nbits += 5;
		if (nbits >= 8) {
			nbits -= 8;
			if (len >= TOTP_KEY_MAX)
				return 0;
			totp_key[len++] = bits >> nbits;
		}
	}
	if (len < 10)	/* refuse trivially short secrets */
		return 0;
	totp_keylen = len;
	return 1;
}


uint8_t totp_verify(__xdata uint8_t *code) __banked
{
	uint8_t i, c;

	if (!totp_enabled || !totp_keylen)
		return 0;
	tv_t = sntp_unix_now();
	if (!tv_t)	/* no wall-clock time: fail closed */
		return 0;
	tv_t /= TOTP_STEP;

	tv_entered = 0;
	for (i = 0; i < 6; i++) {
		c = code[i];
		if (c < '0' || c > '9')
			return 0;
		tv_entered = tv_entered * 10 + (c - '0');
	}
	if (code[6])
		return 0;

	tc_counter = tv_t;
	totp_code();
	if (tc_code == tv_entered)
		return 1;
	tc_counter = tv_t - 1;
	totp_code();
	if (tc_code == tv_entered)
		return 1;
	tc_counter = tv_t + 1;
	totp_code();
	if (tc_code == tv_entered)
		return 1;
	return 0;
}


/* Print tc_code as 6 digits */
static void print_code6(void)
{
	static __xdata char b[7];
	uint8_t i;

	for (i = 6; i; i--) {
		b[i - 1] = '0' + (tc_code % 10);
		tc_code /= 10;
	}
	b[6] = 0;
	print_string_x(b);
}


void totp_status_print(void) __banked
{
	static __xdata uint32_t unix_now;

	print_string("TOTP: ");
	print_string(totp_enabled ? "enabled" : "disabled");
	print_string(", secret: ");
	print_string(totp_keylen ? "set" : "not set");
	unix_now = sntp_unix_now();
	if (!unix_now) {
		print_string(", time not synced (telnet TOTP login fails closed)\n");
		return;
	}
	if (totp_keylen) {
		print_string(", current code: ");
		tc_counter = unix_now / TOTP_STEP;
		totp_code();
		print_code6();
	}
	write_char('\n');
}

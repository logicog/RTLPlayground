#include "httpd/request_buffer.h"

#pragma codeseg BANK4
#pragma constseg BANK4

#define PASSWD_SALT_LEN 4
#define PASSWD_DIGEST_LEN 16
#define PASSWD_MAX_LEN 20

// The last byte stores the hash iteration count and stays zero for plaintext.
static __xdata union {
	char plain[PASSWD_MAX_LEN + 1];
	struct {
		uint8_t salt_digest[PASSWD_SALT_LEN + PASSWD_DIGEST_LEN];
		uint8_t rounds;
	} hash;
} passwd;
_Static_assert(sizeof(passwd.hash) == sizeof(passwd.plain), "password slot size");
__bit passwd_hash_pending;

static bool passwd_plain_matches(__xdata const uint8_t *uri_str_p)
{
	__xdata const char *src_str_p = passwd.plain;
	uint8_t u, s;

	while (1) {
		u = *uri_str_p++;
		s = *src_str_p++;

		if (s == NUL) {
			if (u != NUL && u != ' ' && u != '\t' && u != ':' && u != '?'
			    && u != '=' && u != '\n' && u != '\r') {
				return false;
			}
			return true;
		}

		if (u == '%') {
			u = 0;
			// Preserve the legacy conversion of both percent-encoded digits.
			for (uint8_t i = 0; i < 2; i++) {
				// SDCC uses SWAP for the four-bit rotation.
				u = (u << 4) | (u >> 4);
				uint8_t digit = *uri_str_p++;
				u |= digit <= '9' ? digit - '0' : digit - 'A' + 10;
			}
		} else if (u == '+') {
			u = ' ';
		}

		if (s != u) {
			return false;
		}
	}
}

static int16_t hex_byte(__xdata const uint8_t *p)
{
	uint8_t value = 0;
	for (uint8_t i = 0; i < 2; i++, p++) {
		uint8_t digit = *p - '0';
		if (digit > 9) {
			digit = (*p | 0x20) - 'a';
			if (digit > 5) {
				return -1;
			}
			digit += 10;
		}
		value = (value << 4) | digit;
	}
	return value;
}

// Return the iteration count for a complete record, or zero for a plaintext password.
static uint8_t passwd_hash_load(__xdata const uint8_t *line) __reentrant
{
	// A literal password can start with "blake3 ". Match the full hash record.
	if (!strstart(line, "blake3 ")) {
		return 0;
	}
	line += sizeof("blake3 ") - 1;
	if (*line < '1' || *line > '9') {
		return 0;
	}
	uint8_t iterations = *line++ - '0';
	if (*line >= '0' && *line <= '9') {
		iterations = iterations * 10 + (*line++ - '0');
	}
	if (iterations > 64 || *line++ != ' ') {
		return 0;
	}
	for (uint8_t i = 0; i < sizeof(passwd.hash.salt_digest); i++) {
		if (i == PASSWD_SALT_LEN && *line++ != ' ') {
			return 0;
		}
		int16_t byte = hex_byte(line);
		if (byte < 0) {
			return 0;
		}
		passwd.hash.salt_digest[i] = byte;
		line += 2;
	}
	return *line ? 0 : iterations;
}

void passwd_reset(void) __banked
{
	strtox(passwd.plain, "1234");
	passwd.hash.rounds = 0;
}

void passwd_set(__xdata const uint8_t *value) __banked __reentrant
{
	// A password change also revokes a login still waiting for its reply.
	passwd_hash_pending = 0;
	uint8_t rounds = passwd_hash_load(value);
	passwd.hash.rounds = rounds;
	if (!rounds) {
		uint8_t i = 0;
		while (i < PASSWD_MAX_LEN && *value) {
			passwd.plain[i++] = *value++;
		}
		passwd.plain[i] = NUL;
	}
}

void passwd_hash_cancel(void) __banked
{
	passwd_hash_pending = 0;
	_Static_assert(sizeof(request_buf.password) <= UINT8_MAX, "memset takes an 8-bit length");
	memset((__xdata uint8_t *)&request_buf.password, 0, sizeof(request_buf.password));
}

enum passwd_login_result passwd_login(__xdata const uint8_t *body, uint16_t body_len) __banked __reentrant
{
	static const __code uint8_t prefix[] = "pwd=";
	enum { prefix_len = sizeof(prefix) - 1 };
	if (!strstart(body, prefix)) {
		return PASSWD_LOGIN_REJECTED;
	}
	__xdata const uint8_t *form = body + prefix_len;
	if (!passwd.hash.rounds) {
		return passwd_plain_matches(form) ? PASSWD_LOGIN_ACCEPTED : PASSWD_LOGIN_REJECTED;
	}
	if (body_len <= prefix_len || body_len > prefix_len + 3 * PASSWD_MAX_LEN) {
		return PASSWD_LOGIN_REJECTED;
	}

	// len bounds the encoded field, including when the HTTP body contains a literal NUL.
	uint8_t len = body_len - prefix_len;
	// A buffered form starts at hash.block. Decoding consumes at least one byte before each write.
	_Static_assert(PASSWD_SALT_LEN <= prefix_len, "decoded password must start at or before the form value");
	uint8_t pos = PASSWD_SALT_LEN;
	for (; len; len--) {
		uint8_t c = *form++;
		if (c == '%') {
			if (len < 3) {
				goto invalid;
			}
			int16_t byte = hex_byte(form);
			if (byte < 0) {
				goto invalid;
			}
			c = byte;
			form += 2;
			len -= 2;
		} else if (c == '+' || c == '&') {
			goto invalid;
		}
		if (!c || c == ' ' || (c >= '\t' && c <= '\r') || pos == PASSWD_SALT_LEN + PASSWD_MAX_LEN) {
			goto invalid;
		}
		request_buf.password.hash.block[pos++] = c;
	}
	memcpy(request_buf.password.hash.block, passwd.hash.salt_digest, PASSWD_SALT_LEN);
	request_buf.password.input_len = pos;
	request_buf.password.remaining = passwd.hash.rounds;
	passwd_hash_pending = 1;
	return PASSWD_LOGIN_PENDING;
invalid:
	passwd_hash_cancel();
	return PASSWD_LOGIN_REJECTED;
}

void passwd_hash_step(void) __banked
{
	blake3_digest(&request_buf.password.hash, request_buf.password.input_len);
	request_buf.password.input_len = 32; // Chain the full digest.
	request_buf.password.remaining--;
}

// The HTTP state machine consumes the result after the last iteration.
enum passwd_login_result passwd_hash_finish(void) __banked
{
	uint8_t diff = passwd_hash_pending ? 0 : 1;
	for (uint8_t i = 0; i < PASSWD_DIGEST_LEN; i++) {
		diff |= request_buf.password.hash.block[i] ^ passwd.hash.salt_digest[PASSWD_SALT_LEN + i];
	}
	passwd_hash_cancel();
	return diff ? PASSWD_LOGIN_REJECTED : PASSWD_LOGIN_ACCEPTED;
}

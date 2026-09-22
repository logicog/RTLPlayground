#ifndef __TOTP_H__
#define __TOTP_H__

#include <stdint.h>

#define TOTP_KEY_MAX 32
#define TOTP_STEP 30

extern __xdata uint8_t totp_enabled;
extern __xdata uint8_t totp_keylen;	/* 0 = no secret set */

void totp_init(void) __banked;
/* Decode a base32 secret from the command buffer; returns 1 on success */
uint8_t totp_set_secret(__xdata uint8_t *b32) __banked;
/* Verify a 6-digit code string against T-1, T, T+1; 1 on match */
uint8_t totp_verify(__xdata uint8_t *code) __banked;
void totp_status_print(void) __banked;

#endif

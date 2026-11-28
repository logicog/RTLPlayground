#ifndef PASSWD_HASH_H
#define PASSWD_HASH_H

#include <stdint.h>

extern __bit passwd_hash_pending;

enum passwd_login_result {
	PASSWD_LOGIN_REJECTED,
	PASSWD_LOGIN_ACCEPTED,
	PASSWD_LOGIN_PENDING,
};

void passwd_reset(void) __banked;
void passwd_set(__xdata const uint8_t *value) __banked __reentrant;
enum passwd_login_result passwd_login(__xdata const uint8_t *body, uint16_t len) __banked __reentrant;
void passwd_hash_step(void) __banked;
enum passwd_login_result passwd_hash_finish(void) __banked;
void passwd_hash_cancel(void) __banked;

#endif

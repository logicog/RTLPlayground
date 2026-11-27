#ifndef HTTP_REQUEST_BUFFER_H
#define HTTP_REQUEST_BUFFER_H

#include "rtl837x_common.h"
#include "crypto/blake3.h"

#define CONFIG_UPLOAD_BUF (CONFIG_LEN + 384)

union http_request_buffer {
	uint8_t bytes[CONFIG_UPLOAD_BUF];
	struct {
		struct blake3_work hash;
		uint8_t input_len;
		uint8_t remaining;
	} password;
};
_Static_assert(sizeof(union http_request_buffer) == CONFIG_UPLOAD_BUF,
               "password workspace must fit the request buffer");

extern __xdata union http_request_buffer request_buf;

#endif

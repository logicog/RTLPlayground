#include <stdint.h>
#include "rtl837x_common.h"
#include "page_impl.h"

extern __xdata uint8_t outbuf[TCP_OUTBUF_SIZE];
extern __xdata uint16_t slen;

__code const uint8_t * __code const HTTP_RESPONCE_JSON = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\n\r\n";

// Convert (uint8_t) bool to ascii '0' or '1' char push on html-buffer.
void bool_to_html(char c)
{
	outbuf[slen++] = c ? '1' : '0';
}

//  Convert uint8_t to ascii HEX char.
void byte_to_html(uint8_t val)
{
	outbuf[slen++] = itohex(val >> 4);
	outbuf[slen++] = itohex(val);
}

/* Converts a uint8_t to raw string.
   Suppress leading zeros.
*/
void itoa_html(uint8_t v)
{
	uint8_t t = (v / 100);
	// when print_zeros is not zero, we know that a non-zero number has printed.
	// That have to print all the next numbers.
	uint8_t print_zeros = t;
	if (print_zeros)
		outbuf[slen++] = '0' + t;
	t = (v / 10) % 10;
	print_zeros |= t;
	if (print_zeros)
		outbuf[slen++] = '0' + t;
	outbuf[slen++] = '0' + (v % 10);
}

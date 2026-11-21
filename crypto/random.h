#ifndef _RANDOM_H_
#define _RANDOM_H_

#include <stdint.h>
extern __xdata uint8_t random_64[8];

void get_random_32(void) __banked;
void get_random_64(void) __banked;

#endif

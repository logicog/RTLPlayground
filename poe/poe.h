#ifndef _POE_H_
#define _POE_H_

#include <stdint.h>
#include <stdbool.h>

#define POE_NONE		0x1000
#define RTL8238C		0x8239

#ifndef MACHINE_POE_CHIP
#define MACHINE_POE_CHIP POE_NONE
#endif

#if (MACHINE_POE_CHIP == RTL8238C )
#include "rtl8238c.h"
#elif (MACHINE_POE_CHIP != POE_NONE)
#error "Unsupported MACHINE_POE_CHIP"
#endif

void poe_init(void) __banked;

#endif

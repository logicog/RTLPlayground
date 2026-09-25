/*
 * sdcc_shim.h — neutralize SDCC 8051 keywords so firmware translation units
 * compile under host gcc/clang for off-target unit testing.
 *
 * Force-included (gcc -include) ahead of every firmware source under test.
 * It only erases storage/qualifier keywords and the banked-call attribute;
 * it does NOT change any logic, so the code under test is byte-for-byte the
 * firmware source. Hardware/register access is provided by mocks (support.c),
 * never by this file.
 */
#ifndef SDCC_SHIM_H
#define SDCC_SHIM_H

#include <stdbool.h>
#include <stdint.h>

/* Memory-space qualifiers → nothing (host has one flat address space). */
#define __xdata
#define __code
#define __pdata
#define __idata
#define __data
/* bool, not unsigned char: SDCC's bit types normalize any assigned value
 * to 0/1, which _Bool reproduces exactly. */
#define __bit         bool

/* Special-function registers are declared in rtl837x_sfr.h as
 * `__sfr __at(0xNN) NAME;`. With __at erased those become plain declarations,
 * so they must carry extern or every translation unit including the header
 * defines its own copy. __sbit is the bit-in-an-SFR form and only appears
 * there; the local bit variable keyword is __bit above. test/hw_mock.c
 * undefines these four and re-includes the header once to emit the storage. */
#define __sfr         extern volatile uint8_t
#define __sfr16       extern volatile uint16_t
#define __sfr32       extern volatile uint32_t
#define __sbit        extern bool

/* The firmware ships its own memcpy/memset/strlen/strcpy/sleep, and
 * rtl837x_common.h hides those prototypes under RTLP_HOST_TEST so host builds
 * take the C library versions. Something has to declare them, and firmware
 * sources do call memset and memcpy; this header is force-included ahead of
 * everything, so pulling <string.h> in here puts the libc versions in scope
 * before any firmware header and nothing clashes. sleep() is not in
 * <string.h> and is declared on its own rather than through <unistd.h>. */
#include <string.h>
extern unsigned int sleep(unsigned int seconds);

/* Function attributes → nothing (no banking / no 8051 calling conventions). */
#define __banked
#define __reentrant
#define __naked
#define __using(n)
#define __interrupt(n)
#define __at(addr)

#endif /* SDCC_SHIM_H */

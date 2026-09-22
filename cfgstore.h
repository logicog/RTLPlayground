#ifndef __CFGSTORE_H__
#define __CFGSTORE_H__

#include <stdint.h>

/* In-RAM shadow of the startup configuration. Seeded from flash at boot
 * and after every web config upload; updated on every successful
 * config-shaped CLI/web command with the same merge semantics as the
 * web UI's save button. `save` persists it to the config flash sector. */

/* Matches the web UI's limit for the startup config editor */
#define CFG_SHADOW_SIZE 2048

void cfgstore_load(void) __banked;	/* (re)seed shadow from flash */
void cfgstore_note(void) __banked;	/* merge cmd_buffer's command into shadow */
void cfgstore_save(void) __banked;	/* write shadow to flash + verify */
void cfgstore_show(void) __banked;	/* print shadow */

#endif

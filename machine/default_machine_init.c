/*
 * Fallback boot hook for boards without a machine/<MACHINE>_init.c.
 */
#include "machine_init_defs.h"

void machine_custom_init(void) __banked { }

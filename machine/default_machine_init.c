/*
 * Fallback boot hook for PCB definitions without custom initialization.
 */
#include "machine_defs.h"

#pragma codeseg BANK2
#pragma constseg BANK2
void machine_custom_init(void) __banked { }

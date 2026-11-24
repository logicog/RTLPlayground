#ifndef _MACHINE_DEFAULT_H_
#define _MACHINE_DEFAULT_H_

/*
 * Fallback selected by machine.h when MACHINE_CHIPSET is not defined.
 * Firmware builds provide the chipset in their compiler flags or PCB source;
 * this fallback is for translation units that only need machine.h declarations,
 * notably host-side tests under test/ that define their own `machine` object.
 *
 * FRIENDLY_NAME is deliberately not defined here: a build that reaches this
 * header must not silently pick up some default model string. Any code that
 * needs FRIENDLY_NAME therefore fails to compile unless a real machine is
 * selected.
 */

#define MACHINE_CHIPSET RTL8373

#endif

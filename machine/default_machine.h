#ifndef _MACHINE_DEFAULT_H_
#define _MACHINE_DEFAULT_H_

/*
 * Fallback selected by machine.h when MACHINE_NAME is not defined on the
 * command line. Firmware builds always pass -DMACHINE_NAME=<MACHINE>, so this
 * is used by translation units that only need the machine.h declarations --
 * notably the host-side tests under test/, which define their own `machine`
 * object.
 *
 * MACHINE_MODEL is deliberately not defined here: a build that reaches this
 * header must not silently pick up some default model string. Any code that
 * needs MACHINE_MODEL therefore fails to compile unless a real machine is
 * selected.
 */

#endif

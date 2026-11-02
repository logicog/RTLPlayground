/*
 * PHY driver entry point
 */

#include "machine.h"

#if SOC_FAMILY == RTL836X

#include "rtl836x_phy.c"

#else

#include "rtl837x_phy.c"

#endif

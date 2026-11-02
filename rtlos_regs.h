#ifndef _RTLOS_REGS_H_
#define _RTLOS_REGS_H_

/*
 * Switch register definitions.
 *
 */

#include "machine.h"

#if SOC_FAMILY == RTL836X

#include "rtl837x_regs.h"
#include "rtl836x_regs.h"

#else

#include "rtl837x_regs.h"

#endif

#endif

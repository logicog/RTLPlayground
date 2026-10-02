#ifndef _RTL837X_RMA_H_
#define _RTL837X_RMA_H_

#include <stdint.h>

#define RMA_ENTRIES		20
#define RMA_PTP_PORTS		9

#define RMA_ACT_SHIFT		4
#define RMA_FLAG_STORM		3

void rma_cmd(void) __banked;

#endif

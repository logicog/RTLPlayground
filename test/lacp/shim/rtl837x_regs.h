#ifndef _SHIM_RTL837X_REGS_H_
#define _SHIM_RTL837X_REGS_H_
#define REG_SET(r, v) do { (void)(r); (void)(v); } while (0)
#define RTL837X_RMA2_CONF		0x4ed4
#define RTL837X_RMA_ACT_FORWARD		0x00000000
#define RTL837X_RMA_ACT_DROP		0x00000020
#endif

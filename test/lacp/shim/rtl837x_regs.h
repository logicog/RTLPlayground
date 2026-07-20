#ifndef _SHIM_RTL837X_REGS_H_
#define _SHIM_RTL837X_REGS_H_
#define REG_SET(r, v) do { (void)(r); (void)(v); } while (0)
#define RTL837X_RMA2_CONF		0x4ed4
#define RTL837X_RMA_ACT_FORWARD		0x00000000
#define RTL837X_RMA_ACT_DROP		0x00000020
#endif

/* Host-build shim: registers touched by lacp_send's CPU-port targeting */
#define RTL837X_PORT_ISOLATION_BASE 0x50c0
#define RTL837X_REG_NIC_TXCMD       0x7850

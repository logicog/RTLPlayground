/*
 * Host-build shim for rtl837x_regs.h - the register numbers rtl837x_lacp.c
 * touches, with the values from the real header so a wrong constant shows up
 * as a failing expectation rather than as a silent no-op.
 */
#ifndef _SHIM_RTL837X_REGS_H_
#define _SHIM_RTL837X_REGS_H_

#include <stdint.h>

#define RTL837X_RMA2_CONF		0x4ed4
#define RTL837X_RMA_ACT_TRAP		0x10
#define RTL837X_RMA_ACT_DROP		0x20

/* Registers touched by lacp_send's CPU-port targeting */
#define RTL837X_PORT_ISOLATION_BASE	0x50c0
#define RTL837X_REG_NIC_TXCMD		0x7850

#endif

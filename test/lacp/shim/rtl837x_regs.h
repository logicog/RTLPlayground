/*
 * Host-build shim for rtl837x_regs.h - the register numbers rtl837x_lacp.c
 * touches, with the values from the real header so a wrong constant shows up
 * as a failing expectation rather than as a silent no-op.
 */
#ifndef _SHIM_RTL837X_REGS_H_
#define _SHIM_RTL837X_REGS_H_

#include <stdint.h>

#define REG_SET(r, v) do { (void)(r); (void)(v); } while (0)

/* Register writes are recorded by the harness, which asserts on them. */
void hw_reg_write(uint16_t reg, uint8_t v24, uint8_t v16, uint8_t v8, uint8_t v0);
#define REG_WRITE(r, v24, v16, v8, v0) \
	hw_reg_write((r), (uint8_t)(v24), (uint8_t)(v16), (uint8_t)(v8), (uint8_t)(v0))

#define RTL837X_RMA2_CONF		0x4ed4
#define RTL837X_RMA_ACT_FORWARD		0x00000000
#define RTL837X_RMA_ACT_DROP		0x00000020

/* Registers touched by lacp_send's CPU-port targeting */
#define RTL837X_PORT_ISOLATION_BASE	0x50c0
#define RTL837X_REG_NIC_TXCMD		0x7850

/* L2 table access, used for the static CPU-only LACPDU entry */
#define RTL837X_TBL_CTRL		0x5cac
#define TBL_WRITE			0x02
#define TBL_EXECUTE			0x01
#define TBL_L2_UNICAST			0x04
#define RTL837x_TBL_DATA_IN_A		0x5cb8
#define RTL837x_TBL_DATA_IN_B		0x5cbc
#define RTL837x_TBL_DATA_IN_C		0x5cc0

#endif

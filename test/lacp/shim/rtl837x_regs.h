/* Host-build shim: switch registers are hardware; RMA writes are no-ops here.
 * The harness injects frames directly and does not model the ASIC RMA table,
 * so REG_SET just needs to compile away. */
#ifndef _SHIM_RTL837X_REGS_H_
#define _SHIM_RTL837X_REGS_H_

#define REG_SET(r, v) do { (void)(r); (void)(v); } while (0)

#define RTL837X_RMA2_CONF		0x4ed4
#define RTL837X_RMA_ACT_TRAP_CPU	0x00000010

#endif

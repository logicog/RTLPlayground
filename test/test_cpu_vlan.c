#include <stdint.h>
#include <stdio.h>
#include "rtl837x_common.h"
#include "rtl837x_port.h"
#include "machine.h"
#include "support.h"
#include "hw_mock.h"
extern const struct machine machine;
extern struct machine_runtime machine_detected;
int main(void) {
 hw_reset();out_reset();
 hw_reg_set(RTL837X_VLAN_PORT_EGR_TAG,0xa50fffff);
 hw_reg_set(RTL837x_PVID_BASE_REG+16,0x5a123456);
 vlan_setup();
 uint32_t untouched=(1u<<(MACHINE_MIN_PORT*2))-1;
 CHECK(hw_reg_get(RTL837X_VLAN_PORT_EGR_TAG)==(0xa5000000|untouched),"CPU egress set, unrelated bits preserved");
 CHECK(hw_reg_get(RTL837x_PVID_BASE_REG+16)==0x5a001001,"CPU PVID initialized explicitly");
 uint16_t active=((1u<<(MACHINE_MAX_PORT+1))-1)&~((1u<<MACHINE_MIN_PORT)-1);
 CHECK(hw_reg_get(RTL837X_VLAN_PORT_IGR_FLTR)==(active|PMASK_CPU),"CPU ingress included, excluded user ports rejected");
 CHECK(hw_vlan_word(1)==(machine_detected.isRTL8373?0x0207ffff:0x0207e3f8),"existing family VLAN membership preserved");
 CHECK(hw_vlan_word(2)==0,"no unrelated VLAN changed");
 printf("CPU VLAN min=%u max=%u RTL8373=%u: %d checks, %d failed\n",MACHINE_MIN_PORT,MACHINE_MAX_PORT,machine_detected.isRTL8373,tests_run,tests_failed);
 return tests_failed?1:0;
}

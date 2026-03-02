// #define REGDBG
// #define DEBUG

#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_bandwidth.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata uint8_t sfr_data[4];

void bandwidth_setup(void) __banked
{
	print_string("bandwidth_setup called\n");
	// Exclude all packets possibly for the CPU port, but do not include bypassed packets or Inter-Frame-Gap into bandwidth
	REG_SET(RTL837X_IGBW_CTRL, IGBW_ADM_DHCP | IGBW_ADM_ARPREQ | IGBW_ADM_RMA | IGBW_ADM_BPDU | IGBW_ADM_RTKPKT | IGBW_ADM_IGMP);

	// We do not count IFG for Egress and allways allow CPU-traffic
	REG_SET(RTL837X_EGBW_CTRL, EGBW_CPUMODE);

	print_string("RTL837X_IGBW_CTRL: "); print_reg(RTL837X_IGBW_CTRL); write_char('\n');
	print_string("RTL837X_EGBW_CTRL: "); print_reg(RTL837X_EGBW_CTRL); write_char('\n');
	print_string("bandwidth_setup done\n");
}


/*
 * Set the ingress bandwidth
 * bw: Bandwidth in kb
 */
void bandwidth_ingress_set(uint8_t port, __xdata uint32_t bw) __banked
{
	__xdata uint8_t * __xdata bwptr = &bw;

	print_string("bandwidth_ingress_set called, port "); print_byte(port); write_char('\n');
	sfr_data[0] = 0;
	sfr_data[1] = 0x10 | (*(bwptr + 2) >> 4);  // Set bit 20 to enable ingress bandwidth control
	sfr_data[2] = (*(bwptr + 2) << 4) | (*(bwptr + 1) >> 4);
	sfr_data[3] = (*(bwptr) >> 4) | (*(bwptr + 1) << 4);
	reg_write_m(RTL837X_IGBW_PORT_CTRL + port * 4);

	// We enable Flow Control instead of just dropping packets
	reg_bit_set(RTL837X_IGBW_PORT_FC_CTRL, port);
	print_string("RTL837X_IGBW_PORT_FC_CTRL:"); print_reg(RTL837X_IGBW_PORT_FC_CTRL); write_char('\n');
}


void bandwidth_ingress_drop(uint8_t port) __banked
{
	reg_bit_clear(RTL837X_IGBW_PORT_FC_CTRL, port);
	print_string("RTL837X_IGBW_PORT_FC_CTRL:"); print_reg(RTL837X_IGBW_PORT_FC_CTRL); write_char('\n');
}


void bandwidth_ingress_disable(uint8_t port) __banked
{
	print_string("Ingress bandwidth limit disabled, port "); print_byte(port); write_char('\n');
	REG_SET(RTL837X_IGBW_PORT_CTRL + port * 4, 0x0fffff);
}


void bandwidth_egress_set(uint8_t port, __xdata uint32_t bw) __banked
{
	__xdata uint8_t * __xdata bwptr = &bw;

	print_string("bandwidth_egress_set called, port "); print_byte(port); write_char('\n');
	sfr_data[0] = 0;
	sfr_data[1] = 0x10 | (*(bwptr + 2) >> 4);  // Set bit 20 to enable egress bandwidth control
	sfr_data[2] = (*(bwptr + 2) << 4) | (*(bwptr + 1) >> 4);
	sfr_data[3] = (*(bwptr) >> 4) | (*(bwptr + 1) << 4);
	reg_write_m(RTL837X_EGBW_PORT_CTRL + port * 1024);
}


void bandwidth_egress_disable(uint8_t port) __banked
{
	print_string("Egress bandwidth limit disabled, port "); print_byte(port); write_char('\n');
	REG_SET(RTL837X_EGBW_PORT_CTRL + port * 1024, 0x0fffff);
}


void bandwidth_status(uint8_t port) __banked
{
	print_string("ingress: ");
	reg_read_m(RTL837X_IGBW_PORT_CTRL + port * 4);
	if (sfr_data[1] & 0x10) {
		print_string("enabled: ");
		sfr_data[1] &= 0xef;
		print_sfr_data();
		write_char('\n');
	} else {
		print_string("disabled\n");
	}

	print_string("egress: ");
	reg_read_m(RTL837X_EGBW_PORT_CTRL + port * 1024);
	if (sfr_data[1] & 0x10) {
		print_string("enabled: ");
		sfr_data[1] &= 0xef;
		print_sfr_data();
		write_char('\n');
	} else {
		print_string("disabled\n");
	}
}

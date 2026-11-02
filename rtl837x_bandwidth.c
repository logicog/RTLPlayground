// #define REGDBG
// #define DEBUG

#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtlos_regs.h"
#include "rtl837x_bandwidth.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata uint8_t sfr_data[4];

void bandwidth_setup(void) __banked
{
	print_string("bandwidth_setup called\n");
#if SOC_FAMILY == RTL836X
	/* The RTL836x has the per-port ingress limiter (0x000f + port*0x20)
	 * but no equivalent of the RTL8372/3 global admission or egress-control
	 * registers, so there is nothing to arm here: the limit is set
	 * per port by bandwidth_ingress_set(). */
	print_string("bandwidth_setup done\n");
#else
	// Exclude all packets possibly for the CPU port, but do not include bypassed packets or Inter-Frame-Gap into bandwidth
	REG_SET(RTL837X_IGBW_CTRL, IGBW_ADM_DHCP | IGBW_ADM_ARPREQ | IGBW_ADM_RMA | IGBW_ADM_BPDU | IGBW_ADM_RTKPKT | IGBW_ADM_IGMP);

	// We do not count IFG for Egress and allways allow CPU-traffic
	REG_SET(RTL837X_EGBW_CTRL, EGBW_CPUMODE);

	print_string("RTL837X_IGBW_CTRL: "); print_reg(RTL837X_IGBW_CTRL); write_char('\n');
	print_string("RTL837X_EGBW_CTRL: "); print_reg(RTL837X_EGBW_CTRL); write_char('\n');
	print_string("bandwidth_setup done\n");
#endif
}


/*
 * Set the ingress bandwidth
 * bw: Bandwidth in kb
 */
void bandwidth_ingress_set(uint8_t port, __xdata uint32_t bw) __banked
{
#if SOC_FAMILY == RTL836X
	uint32_t rate = bw & RTL836x_IGBW_RATE_MASK;	/* 17 bits */
	uint16_t misc;

	REG_SET(RTL836x_IGBW_RATE_LO_REG(port), (uint16_t)(rate & 0xffff));
	REG_SET(RTL836x_IGBW_RATE_HI_REG(port), (uint16_t)((rate >> 16) & 1));

	reg_read_m(RTL836x_PORT_MISC_CFG_REG(port));
	misc = (uint16_t)(((uint16_t)sfr_data[2] << 8) | sfr_data[3]);
	misc &= (uint16_t)~((uint16_t)1 << RTL836x_IGBW_MISC_FC_OFFSET);
	misc |= (uint16_t)1 << RTL836x_IGBW_MISC_FC_OFFSET;	/* flow control, not drop */
	REG_SET(RTL836x_PORT_MISC_CFG_REG(port), misc);
	return;
#else
	__xdata uint8_t * __xdata bwptr = &bw;

	print_string("bandwidth_ingress_set called, port "); print_byte(port); write_char('\n');
	sfr_data[0] = 0;
	sfr_data[1] = 0x10 | (*(bwptr + 2) >> 4);  // Set bit 20 to enable ingress bandwidth control
	sfr_data[2] = (*(bwptr + 2) << 4) | (*(bwptr + 1) >> 4);
	sfr_data[3] = (*(bwptr) >> 4) | (*(bwptr + 1) << 4);
	reg_write_m(RTL837X_IGBW_PORT_CTRL + port * 4);

	// We enable Flow Control instead of just dropping packets
	reg_bit_set(RTL837X_IGBW_PORT_FC_CTRL, port);
#endif
}


void bandwidth_ingress_drop(uint8_t port) __banked
{
	reg_bit_clear(RTL837X_IGBW_PORT_FC_CTRL, port);
	print_string("RTL837X_IGBW_PORT_FC_CTRL:"); print_reg(RTL837X_IGBW_PORT_FC_CTRL); write_char('\n');
}


void bandwidth_ingress_fc(uint8_t port) __banked
{
	reg_bit_set(RTL837X_IGBW_PORT_FC_CTRL, port);
	print_string("RTL837X_IGBW_PORT_FC_CTRL:"); print_reg(RTL837X_IGBW_PORT_FC_CTRL); write_char('\n');
}


void bandwidth_ingress_disable(uint8_t port) __banked
{
	print_string("Ingress bandwidth limit disabled, port "); print_byte(port); write_char('\n');
#if SOC_FAMILY == RTL836X
	REG_SET(RTL836x_IGBW_RATE_LO_REG(port), RTL836x_IGBW_RATE_DISABLE);
	REG_SET(RTL836x_IGBW_RATE_HI_REG(port), RTL836x_IGBW_RATE_DISABLE >> 16);
#else
	REG_SET(RTL837X_IGBW_PORT_CTRL + port * 4, 0x0fffff);
#endif
}


void bandwidth_egress_set(uint8_t port, __xdata uint32_t bw) __banked
{
	__xdata uint8_t * __xdata bwptr = &bw;

	print_string("bandwidth_egress_set called, port "); print_byte(port); write_char('\n');
#if SOC_FAMILY == RTL836X
	(void)bwptr;
	print_string("egress bandwidth limit is not available on this chip\n");
	return;
#else
	sfr_data[0] = 0;
	sfr_data[1] = 0x10 | (*(bwptr + 2) >> 4);  // Set bit 20 to enable egress bandwidth control
	sfr_data[2] = (*(bwptr + 2) << 4) | (*(bwptr + 1) >> 4);
	sfr_data[3] = (*(bwptr) >> 4) | (*(bwptr + 1) << 4);
	reg_write_m(RTL837X_EGBW_PORT_CTRL + port * 1024);
#endif
}


void bandwidth_egress_disable(uint8_t port) __banked
{
	print_string("Egress bandwidth limit disabled, port "); print_byte(port); write_char('\n');
#if SOC_FAMILY == RTL836X
	(void)port;
	print_string("egress bandwidth limit is not available on this chip\n");
#else
	REG_SET(RTL837X_EGBW_PORT_CTRL + port * 1024, 0x0fffff);
#endif
}


void bandwidth_status(uint8_t port) __banked
{
	print_string("ingress: ");
#if SOC_FAMILY == RTL836X
	{
		uint16_t lo, hi;

		reg_read_m(RTL836x_IGBW_RATE_LO_REG(port));
		lo = (uint16_t)(((uint16_t)sfr_data[2] << 8) | sfr_data[3]);
		reg_read_m(RTL836x_IGBW_RATE_HI_REG(port));
		hi = (uint16_t)(((uint16_t)sfr_data[2] << 8) | sfr_data[3]) & 1;
		/* 17-bit value: the compare must not be done in 16 bits. */
		if ((((uint32_t)hi << 16) | lo) == RTL836x_IGBW_RATE_DISABLE) {
			print_string("disabled\n");
		} else {
			print_string("enabled: 0x");
			print_byte(hi);
			print_byte(lo >> 8);
			print_byte(lo);
			write_char('\n');
		}
	}
	print_string("egress: not present on this chip\n");
#else
	reg_read_m(RTL837X_IGBW_PORT_CTRL + port * 4);
	if (sfr_data[1] & 0x10) {
		print_string("enabled: ");
		sfr_data[1] &= 0xef;
		print_string("0x");
		print_byte(sfr_data[1]);
		print_byte(sfr_data[2]);
		print_byte(sfr_data[3]);
		write_char('0');
		write_char('\n');
	} else {
		print_string("disabled\n");
	}

	print_string("egress: ");
	reg_read_m(RTL837X_EGBW_PORT_CTRL + port * 1024);
	if (sfr_data[1] & 0x10) {
		print_string("enabled: ");
		sfr_data[1] &= 0xef;
		print_string("0x");
		print_byte(sfr_data[1]);
		print_byte(sfr_data[2]);
		print_byte(sfr_data[3]);
		write_char('0');
		write_char('\n');
	} else {
		print_string("disabled\n");
	}
#endif	/* SOC_FAMILY != RTL836X: RTL836x has no egress limiter */
}

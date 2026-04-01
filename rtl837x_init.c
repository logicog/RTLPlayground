#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_port.h"
#include "rtl837x_phy.h"
#include "phy.h"
#include "machine.h"

extern __xdata uint8_t sfr_data[4];
extern __code struct machine machine;
extern __xdata struct machine_runtime machine_detected;

#pragma codeseg BANK2
#pragma constseg BANK2

/*
 * Configure the PHY-Side of the SDS-SDS link between SoC and PHY
 */
void sds_init(void)
{
/*
	p001e.000d:9535 R02f8-00009535 R02f4-0000953a P000001.1e00000d:953a
	p001e.000d:953a p001e.000d:953a R02f8-0000953a R02f4-00009530 P000001.1e00000d:9530

	RTL8373:
	p001e.000d:0010 R02f8-00000010 R02f4-0000001a P000001.1e00000d:b7fe
	p001e.000d:0010 p001e.000d:0010	R02f8-00000010 R02f4-00000010 P000001.1e00000d:b7fe
*/
	phy_read(0, PHY_MMD30, 0xd);
	uint16_t pval = SFR_DATA_U16;

	// PHY Initialization:
	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);
	delay(20);

	pval &= 0xfff0;
	pval |= 0x0a;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);
	delay(10);

	phy_write_mask(0x1, PHY_MMD30, 0xd, pval);

	phy_read(0, PHY_MMD30, 0xd);
	pval = SFR_DATA_U16;

	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);

	pval &= 0xfff0;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);

	phy_write_mask(0x1, PHY_MMD30, 0xd, pval);

	if (machine_detected.isN) {
		uint16_t pval;

		print_string("  N-settings");
		if (machine.n_10g)
			print_string(" - 10g");
		// Serdes 0 RX PN swap for 64B/66B
		sds_read(1, 6, 2);
		pval = SFR_DATA_U16;
		sds_write_v(1, 6, 2, pval | 0x2000);

		// Serdes 1 RX PN swap for 8B/10B
		sds_read(1, 0, 0);
		pval = SFR_DATA_U16;
		sds_write_v(1, 0, 0, pval | 0x200);

		// Serdes 0 RX PN swap for 64B/66B
		sds_read(0, 6, 2);
		pval = SFR_DATA_U16;
		sds_write_v(0, 6, 2, pval | 0x2000);

		if (!machine.n_10g) {
			if (machine_detected.isRTL8373) {
				// RTL8224: Serdes 0 RX PN swap for 64B/66B
				// We assume that RTL8373N always paired with RTL8224N.
				// This sds register value is 0x0000 at reset.
				// So only write to it.
				RTL8224_SDS_WRITE(0, 6, 2, 0x2000);
			} else {
				// Serdes 0 RX PN swap for 8B/10B
				sds_read(0, 0, 0);
				pval = SFR_DATA_U16;
				sds_write_v(0, 0, 0, pval | 0x200);
			}
		}
		if (machine.n_10g) {
			reg_read_m(RTL837X_CFG_PHY_MDI_REVERSE);
			sfr_mask_data(0, 0x0f,0x0c);
			reg_write_m(RTL837X_CFG_PHY_MDI_REVERSE);
			REG_SET(RTL837X_CFG_PHY_TX_POLARITY_SWAP, 0x0000596a);
		}
	}
}


void rtl8373_init(void) __banked
{
	print_string("\nrtl8373_init called\n");

	// r65d8:3ffbedff R65d8-3ffbedff
	reg_bit_set(0x65d8, 0x1d);

	sds_init();
	// Disable all SERDES for configuration
	REG_SET(RTL837X_REG_SDS_MODES, 0x000037ff);

	// q000601:c800 Q000601:c804 q000601:c804 Q000601:c800
	sds_read(0, 0x06, 0x01);
	uint16_t pval = SFR_DATA_U16;
	sds_write_v(0, 0x06, 0x01, pval | 0x04);
	delay(50);
	sds_read(0, 0x06, 0x01);
	pval = SFR_DATA_U16;
	sds_write_v(0, 0x06, 0x01, pval & 0xfffb);

	phy_config_8224();
	sds_config_mac(1, SDS_OFF);    // Off for now until SFP+ port used
	sds_config_mac(2, SDS_SGMII);  // For RTL8224
	sds_config(0, SDS_QXGMII);     // For RTL8224

	// SDS 1 setup
	// q012100:4902 Q012100:4906 q013605:0000 Q013605:4000 Q011f02:001f q011f15:0086
	sds_write_v(1, 0x21, 0x00, 0x4906);
	sds_write_v(1, 0x36, 0x05, 0x4000);
	sds_write_v(1, 0x1f, 0x02, 0x001f);
	sds_read(1, 0x1f, 0x15);
	pval = SFR_DATA_U16;

	// r0a90:000000f3 R0a90-000000fc
	reg_read_m(RTL837X_CFG_PHY_MDI_REVERSE);
	sfr_mask_data(0, 0x0f,0x0c);
	reg_write_m(RTL837X_CFG_PHY_MDI_REVERSE);

	if (machine_detected.isN) {
		print_string("  TX_POLARITY_SWAP\n");
		// FOR N-Version: #TX_POLARITY_SWAP
		reg_read_m(RTL837X_CFG_PHY_TX_POLARITY_SWAP);
			sfr_data[2] = 0x59;
			sfr_data[3] = 0x6a;
		reg_write_m(RTL837X_CFG_PHY_TX_POLARITY_SWAP);
	}

	rtl8224_phy_enable();

	// Disable PHYs for configuration
	phy_write_mask(0xff,PHY_MMD31,0xa610,0x2858);

	// Set bits 0x13 and 0x14 of 0x5fd4
	// r5fd4:0002914a R5fd4-001a914a
	reg_bit_set(0x5fd4, 0x13);
	reg_bit_set(0x5fd4, 0x14);

	// Configure ports
	uint16_t reg = 0x1238; // Port base register for the bits we set
	for (char i = 0; i < 9; i++) {
		// Bit 7 (0x40) enables replacement of the RTL-VLAN tag with an 802.1Q VLAN tag
		REG_SET(reg, 0xe77);
		reg += 0x100;
	}

	// r0b7c:000000d8 R0b7c-000000f8 r6040:00000030 R6040-00000031
	reg_bit_set(0xb7c, 5);

	// R7124-00001050 R7128-00001050 R712c-00001050 R7130-00001050 R7134-00001050 R7138-00001050
	// R713c-00001050 R7140-00001050 R7144-00001050 R7148-00001050
	REG_SET(0x7124, 0x1050); REG_SET(0x7128, 0x1050); REG_SET(0x712c, 0x1050);
	REG_SET(0x7130, 0x1050); REG_SET(0x7134, 0x1050); REG_SET(0x7138, 0x1050);
	REG_SET(0x713c, 0x1050); REG_SET(0x7140, 0x1050); REG_SET(0x7144, 0x1050);
	REG_SET(0x7148, 0x1050);

	reg_bit_set(RTL837X_REG_HW_CONF, 0);

	// enable EEE for all ports at highest speed possible, but don't reset the PHYs
	port_eee_enable_all(EEE_NORESET);

	// TODO: patch the PHYs

	// Re-enable PHY after configuration
	phy_write_mask(0xff,PHY_MMD31,0xa610,0x2058);

	// Enables MAC access
	// Set bits 0xc-0x14 of 0x632c to 0x1f8, see rtl8372_init
	// r632c:00000540 R632c-001f8540 // RTL8373: 001ff540
	reg_read_m(0x632c);
	sfr_mask_data(1, 0x70, 0xf0); // The ports of the RTL8824
	sfr_mask_data(2, 0x10, 0x1f);
	reg_write_m(0x632c);

	print_string("\nrtl8373_init done\n");
}


void rtl8372_init(void) __banked
{
	print_string("\nrtl8372_init called\n");

	sds_init();
	phy_config(8);	// PHY configuration: External 8221B?
	if (machine.n_10g)
		phy_config_8261(3);
	else
		phy_config(3);	// PHY configuration: all internal PHYs?
	// Set the MAC SerDes Modes Bits 0-4: SDS 0 = 0x2 (0x2), Bits 5-9: SDS 1: 1f (off)
	// r7b20:00000bff R7b20-00000bff r7b20:00000bff R7b20-00000bff r7b20:00000bff R7b20-000003ff r7b20:000003ff R7b20-000003e2 r7b20:000003e2 R7b20-000003e2
	if (machine.n_10g) {
		REG_SET(RTL837X_REG_SDS_MODES, 0x3ed); // Disable SFP for now, set RTL8226 SDS 0 to 0xd
	} else {
		reg_read_m(RTL837X_REG_SDS_MODES);
		sfr_mask_data(1, 0, 0x03);
		sfr_mask_data(0, 0, 0xe2);
		reg_write_m(RTL837X_REG_SDS_MODES);
	}

	// r0a90:000000f3 R0a90-000000fc
	reg_read_m(RTL837X_CFG_PHY_MDI_REVERSE);
	sfr_mask_data(0, 0x0f, 0x0c);
	reg_write_m(RTL837X_CFG_PHY_MDI_REVERSE);

	// Disable PHYs for configuration
	phy_write_mask(0xf0,PHY_MMD31,0xa610,0x2858);

	// Set bits 0x13 and 0x14 of 0x5fd4
	// r5fd4:0002914a R5fd4-001a914a
	reg_bit_set(0x5fd4, 0x13);
	reg_bit_set(0x5fd4, 0x14);

	// Configure ports 3-8:
	//
	// r1538:00000e33 R1538-00000e37 r1538:00000e37 R1538-00000e37 r1538:00000e37 R1538-00000f37
	// [...]
	///
	uint16_t reg = 0x1238 + 0x300; // Port base register for the bits we set
	for (char i = machine.min_port; i <= machine.max_port; i++) {
		// Bit 7 (0x40) enables replacement of the RTL-VLAN tag with an 802.1Q VLAN tag
		REG_SET(reg, 0xe77);
		reg += 0x100;
	}

	// r0b7c:000000d8 R0b7c-000000f8 r6040:00000030 R6040-00000031
	reg_bit_set(0xb7c, 5);

	reg_bit_set(RTL837X_REG_HW_CONF, 0);


	// enable EEE for all ports at highest speed possible, but don't reset the PHYs
	port_eee_enable_all(EEE_NORESET);
	
	// TODO: patch the PHYs

	// Re-enable PHY after configuration
	phy_write_mask(0xf0,PHY_MMD31,0xa610,0x2058);

	// Enables MAC access
	// Set bits 0xc-0x14 of 0x632c to 0x1f8, see rtl8372_init
	// r632c:00000540 R632c-001f8540 // RTL8373: 001ff540
	reg_read_m(0x632c);
	sfr_mask_data(1, 0x70, 0x80);
	sfr_mask_data(2, 0x10, 0x1f);
	reg_write_m(0x632c);
	print_string("\nrtl8372_init done\n");
}

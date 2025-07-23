# SFP+ Slots

The RTL8372/3 provide support for 1 or 2 SFP+ slots, which support fiber and Ethernet
module with speeds of 1GBit, 2.5GBit and 10GBit. 5GBit could be possible but is not
implemented due to the lack of suitable modules.

When a module is inserted, it directly connects to GPIO, I2C and RX/TX data lines of
the SoC. An example schematics can be found here:
(SFP Module Schematics)[https://sfp.by/source/manual/SCP6F44-GL-BWE.pdf]. Another
resources is (here)[https://www.sfptransceiver.com/product_pdf/SFP/SFP%20Design%20Guide.pdf].
The SoC
detects the insertion because the MOD-DEF0 line is pulled low by the module. The
corresponding bit in RTL837X_REG_GPIO_B or RTL837X_REG_GPIO_C will transition from
1 to 0. At that point, the code waits for some 100ms in order for the module to power
up and then reads the EEPROM of the module to get the type of module and in particular
the bit-rate. The EEPROM can be read via the MOD-DEF1 and MOD-DEF2 lines which
provide a standard I2C interface to the standard 24C-EEPROM. The SoCs contain a simple
I2C controller for reading such EEPROMs so that interfacing is very simple.

## I2C Controller

The I2C controller of the RTL8372/3 is very simple and probably designed specifically
for reading 24C EEPROMs. Its use is straight-forward: Configure the I2C bus used
(the code currently uses the default already set regarding what is probably timing)
in the RTL837X_REG_I2C_CTRL register. Then set the EEPROM-register's addresss to be
read in RTL837X_REG_I2C_IN (least-significant byte). The I2C transfer is started
by setting the 0-bit of RTL837X_REG_I2C_CTRL. When this bit is cleared by the
ASIC-side of the SoC, the resulting value can be read in the LSB of RTL837X_REG_I2C_OUT.
This is the code:
```
uint8_t sfp_read_reg(uint8_t slot, uint8_t reg)
{
        // Select I2C-bus according to slot
	if (slot == 0) {
		reg_read_m(RTL837X_REG_I2C_CTRL);
		sfr_mask_data(1, 0xff, 0x72);
		reg_write_m(RTL837X_REG_I2C_CTRL);
	} else {
		reg_read_m(RTL837X_REG_I2C_CTRL);
		sfr_mask_data(1, 0xff, 0x6e);
		reg_write_m(RTL837X_REG_I2C_CTRL);
	}

	REG_WRITE(RTL837X_REG_I2C_IN, 0, 0, 0, reg);

	// Execute I2C Read
	reg_bit_set(RTL837X_REG_I2C_CTRL, 0);

	// Wait for execution to finish
	do {
		reg_read_m(RTL837X_REG_I2C_CTRL);
	} while (sfr_data[3] & 0x1);

	reg_read_m(RTL837X_REG_I2C_OUT);
	return sfr_data[3];
}
```

The description of the data stored in the EEPROM can be found in the
(SFF-8472 standard)[https://members.snia.org/document/dl/25916]
The most relevant is byte 12 (0x0c), which gives the signalling rate of the module in
100MBit, including the 25% overhead for error correction. Currently the code looks like
this:
```
static inline uint8_t sfp_rate_to_sds_config(register uint8_t rate)
{
	if (rate == 0xd)
		return SDS_1000BX_FIBER;
	if (rate == 0x1f)  // Ethernet 2.5 GBit
		return SDS_HSG;
	if (rate > 0x65 && rate < 0x70)
		return SDS_10GR;
	return 0xff;
}
```
For example, a 1000MBit fiber module will have a rate coding of 0xd = 13 = 1300Mbit,
which is the rounded-up value for 1250MBit, the error-corrected bit-rate of
a 1000BX fiber module.

## Interfacing the module for RX/TX

In order to transmit data or receive data from the module, the SerDes of the SoC connected
to the module needs to e properly configured. As can be seen from the
(SFP Module Schematics)[https://sfp.by/source/manual/SCP6F44-GL-BWE.pdf], the Photo-transistor
of the module is optimized by an amplifier and quantized to bits, which directly arrive
at the SoC in a differential pair. This data still has the 25% overhead of the error correction
codes that were on the fiber. The switch needs to configure the SerDes correctl (sds_config())
and set up the MAC on the SoC to talk to the SDS with the correct bit-rate.

## Other SFP-module GPIOs
SFP modules also provide RX-LOS GPIOs, which pulls low when the fiber or Ethernet
cable is not attached (on either side of the link) and usually also a TX-disable GPIO,
which allows to disable the Laser in order to power down the link. There is typically
also a TX-Fault GPIO which pulls low when the laser overheated. While the RX-Los pin
is connected to the SoC and can be read for the devices with a single SFP+ slot
(for the dual-SFP+ slot KP-9000-6HX-X2 only the RX-LOS pin of the right slot seems to
be connected), the other GPIOs have not been identified and counting lines on the PCB
seems to indicate these pins are unlikely to be connected.

The RX-LOS GPIO does not provide any further benefit, since the link status can also be
read from the link-status registers of the MAC or SDS.

The easiest way to identifiy additional GPIOs of an SFP module is to take a cheap module
apart, solder wires to the pins of the on-board PCB which are then routed back through
the end of the module. By pulling e.g. TX-Fault low while printing out the GPIOs, the
correct GPIO can be identified.


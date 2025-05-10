# RTLPlayground
A Playground for Firmware development for RTL8372/RTL8373 based 2.5GBit Switches

## Compiling
Install the following particular build requisites (Debian 12, should work on Ubuntu)
```
sudo apt install sdcc xxd
```

Now, building the firmware image should work:
```
$ make
sdas8051 -plosgff crtstart.asm
sdcc -mmcs51 -c rtlplayground.c
sdcc -mmcs51 -c rtl837x_flash.c
sdcc -mmcs51 -Wl-bHOME=0x100 -o rtlplayground.ihx crtstart.rel rtlplayground.rel rtl837x_flash.rel
objcopy --input-target=ihex -O binary rtlplayground.ihx rtlplayground.img
if [ -e rtlplayground.bin ]; then rm rtlplayground.bin; fi
echo "0000000: 00 40" | xxd -r - rtlplayground.bin
cat rtlplayground.img >> rtlplayground.bin 
```
Note, that the image generated ends in .bin, not .img, in order to make
IMSProg happy.

## Installation
You can play with the image using ghidra or flash real Switch Hardware

### Supported Hardware
If you do not have an RTL837x-based switch device such as the ones
mentionned here: [Up-N-Atoms 2.5 GBit RTL Switch hacking guide]
(https://github.com/up-n-atom/SWTG118AS) or one of the other that
deployment was tested on, including:
- keepLINK kp-9000-6hx-x2 (RTL8372: 4x 2.5GBit + 2x 10GBit SFP+)
- keepLINK KP-9000-6XHML-X2, same as above, but Managed
- keepLINK kp-9000-6hx-x (RTL8372 + RTL8221B 2.5GBit PHY: 5 x 2.5GBit + 1x 10GBit SFP+)
- keepLINK kp-9000-9xh-x-eu (2 x RTL8373, one slaved to the other via MDIO: 8x 2.5GBit + 1x 10GBit SFP+)
- Lianguo LG-SWTGW218AS (2 x RTL8373, one slaved to the other via MDIO: 8x 2.5GBit + 1x 10GBit SFP+)
- No-Name ZX-SWTGW215AS, same but Managed
- No-Name ZX-SWTGW215AS, managed version of kp-9000-6hx-x, ordered on
  AliExpress as keepLINK 5+1 port managed

you can use ghidra to look at the image layout to understand how the image
is organized.

### Understanding the image using ghidra
Start ghidra, load file starting from offset 0x0002 into
memory starting at 0x0000. The lengthe is 0x10000. Select generic 8051, big
endian.

After loading, the boot vector is at 0x0000, which will jump to 0x0100 for
the boot routine.

The firmware uses only bank 1 of the RTL837x since it is quite short.
Otherwise the firmware would be organized as follows
```
--------------------------- 0x0000 ---------------------------------
Boot-Vector
ISRs
Common Code
Trampoline for inter-bank calls
Inter-bank calls, calling trampoline, one for each callable function

----- Bank 1 0x4000 ------   ---- Bank 2 0x4000 -----  -------- .....
Overlay 1                    Overlay 2                 Overlay n

--------- 0xffff ---------   -------- 0xffff --------  -------- 0xffff
```

### Hardware supported by the code so far

- The following hardware is supported:
- Clock generation, including different divider settings
- Interrupt control for timer, serial, external irqs 0, 1
- Serial console via SFRs
- Flash operations via SFRs
- Access to Switch registers via SFRs
  - LED setup
  - Reset
  - Some switch settings
- Access to PHYs via MDIO (only conceptually, not tested):
  - Clause 22? via SFR
  - Clause 45 via SFR

Access to CPU-Port NIC via a different set of SFRs and external IRQ1 is conceptually understood, but not
included because without proper switch register configuration it cannot be
tested.

### Installation on an actual switch

NOTE THAT WHILE THIS PROCEDURE HAS BEEN SUCCESSFULLY TESTED ON ALL DEVICES ABOVE,
ABSOLUTELY NO WARRANTY CAN BE GIVEN THAT YOU WILL NOT DESTROY YOUR SWITCH,
ANY OTHER EQUIPMENT INVOLVED OR HARM YOURSELF.

There is no support for uploading the firmware via ethernet. Instead you
need to open the switch and flash the image directly onto the flash chip,
which is done easiest using a SOIC-8 clip (alternatively you de-solder the
flash chip and install a SOIC adapter):
- Disconnect power from switch
- Attach the clip onto the flash chip
- Connect USB of flash programmer, the power LED on the switch will light
  up, check cabling if not. Don't panic, mixing up GND and 3.3V does not
  seem to destroy the switch (at leasts the on I did this to).
- Use IMSProg (flashrom should work, too) to detect the clip
- MAKE A BACKUP OF THE EXISTING FIRMWARE!
- then load the firmware into IMSProg
- and program flash

Now you can connect a serial cable to the UART port found on all the
devices, set 8N1 @ 57600 baud and power up the switch.

The device will perform some examples and provide a minimal console, which
currently only allows to do a reset.

Enjoy playing!

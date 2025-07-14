# RTLPlayground
A Playground for Firmware development for RTL8372/RTL8373 based 2.5GBit Switches.

It provides a minimal alternative firmware for the unmanaged switches. At this point,
the firmware can be installed on the hardware as given below, how much of the switch
will actually work as a switch, will vary. On the 
keepLINK kp-9000-6hx-x (RTL8372 + RTL8221B 2.5GBit PHY: 5 x 2.5GBit + 1x 10GBit SFP+),
at present the system will provide the same featurs as a dumb switch plus a tiny
TCP stack that will allow to reply to ARP and ping messages, thus enabling pinging the device.
The ports served by the RTL8372 will be 100M/1G/2.5G auto-detect. Port 5 to RTL8221B PHY
SerDes configuration works and supports 1GBit and 2.5GBit Ethernet (SGMII/HISGMII).
SFP module insert/removal identification and reading of the SFP EEProm works. SFP
module configuration works, too, tested for 1G modules.

On the 9-port devices with RTL8273 + RTL8224, the 4 Ports served by the RTL8273 and
the SFP+ port will work normally and TCP connectivity will work as above. The RTL8224
is currently not correctly initialized.

On the 4-Port Ethernet + 2 Port SFP+ devices, one of the SFP+ ports will not work.

To do meaningful debugging you will need to use a serial console, so soldering skills
are required. Flashing must be done via a SOIC-8 PatchClamp or by soldering a socket
for the flash chip.

UPDATE: The Code comes with a port of the [uIP](https://github.com/adamdunkels/uip)
TCP/IP stack and includes a minimal web-server that can be used to work with the switch,
so if you use a patch-clamp for updating the firmware (~3 USD/EUR), you can try this
out without the need to solder anything. See the instructions below.
Note that updating the firmware of a managed switch with the images created in this
project will not work, because it is currently unknown how to generate the require
checksum, see this
[issue](https://github.com/up-n-atom/SWTG118AS/issues/4).

If you don't want to open your device, you can use the project's code to learn about the
devices by looking at the image using e.g. Ghidra.

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
- Lianguo LG-SWTGW218AS (RTL8373 + RTL8224 PHY: 8x 2.5GBit + 1x 10GBit SFP+)
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
The RTL837x firmware images are organized as follows:
The first 2 bytes of the image give the size of the prefetched data at the
start of the CPU power up. The default is 0x4000 (bytes: 0x00 0x40), which
means that the entire shared area of the code memory in all banks,
0x4000 bytes is read immediately into the code RAM.

Common code starts at
0x0002 in the image and has length 0x3ffd, the first bank starts at 0x4000
in the image, is mapped to 0x4000 and has length 0xc000. The second bank
starts at 0x10000, is mapped to 0x4000 and has length 0xc000. The third
bank would start at 0x1c000 and would again be mapped to 0x4000.
There are about 30 banks in use for managed switches, unmanaged ones use
2-3, while the hardware would allow to use 0x3f banks, i.e. up to 4 MB of
flash.

The current image uses Common BANK0 and the first BANK1 via sdccs __banked
function keyword and custom banking trampoline code for the RTL837x in
assembler.


### Hardware supported by the code so far

- The following hardware is supported:
- Clock generation, including different divider settings
- Interrupt control for timer, serial, external irqs 0, 1
- Serial console via SFRs
- Flash operations via SFRs
- Bank switching via SFRs
- Access to Switch registers via SFRs
  - LED setup
  - Reset
  - Some switch settings such as MAC configuration
  - GPIO to detect SFP module insert/removal/LOS
  - I2C to read SFP EEPROM
  - NIC setup
  - L2 learning table access, L2 table flushing
  - VLAN setup/configuration
  - Port mirroring
- Access to PHYs via MDIO (only conceptually, not tested):
  - SerDes settings of SoC via SFR: Configure SFPs in 10Gbit/2.5Gbit/1Gbit, RTL8221
  - Clause 45 via SFR: configure RTL8221
- NIC TX and RX of packets via SFRs
  - send and receive Ethernet frames via SFRs and Switch registers

Ethernet frame RX IRQ via IRQ1 is conceptually understood, but not activated. RX is
currently done via polling, which allows ping-times of <100ms.

The RTL8372/3 have 256 bytes of internal RAM (INTMEM) accessible through MOV
instructions, which are used for the stack and important globals. Some of
these are bit-adressable, e.g. for storing global flags.

Additionally, 64kB of extended RAM (XMEM) is built in, which is accessed
through the MOVX instruction. It is used for global variables, for most
of the function argument passing that is not done using the 8 registers
R0-R7 or registers A/B, and for local variables (which requires extremely
careful planning). The flash memory is transparently accessible for code
being executed and can be used to store configuration. Access is done through
the MOVC instruction, possibly setting the bank register before and
resetting it to access the entire 4MB space. Code is prefetched from flash
and cached in a small RAM automatically by the HW.

The peripherial functions are accessed through 2 different mechanisms:
- Special Function Registers (SFRs, 0x80-0xff) for banking, timers, UART, access to
  switch registers, MDIO, SPI (flash) and NIC transfers. Some SFRs are not
  used for HW purposes and can be used as RAM. Some SFRs are bit-adressable,
  allowing for very tight event wait loops (a single 2-byte instruction).
- 0x10000 switch registers, which appear to be very similar to the registers
  of the RTL838x, for which source code and datasheets are available. This
  controls clock dividers, GPIO/LEDs and general  switch functionality. 
There should be an I2C controller that reads the SFP EEPROMs, but it is not
clear whether this is bit-banged GPIO like for the RTL83xx or dedicated HW
as for the RTL93xx.

The playground image shows access to the different types of memory using the
SDCC compiler. Any support of Linux or e.g. Zephyr would require porting gcc.
There are FreeRTOS ports to 8051 processors using sdcc, however.


### Installation on an actual switch

> [!CAUTION]
> NOTE THAT WHILE THIS PROCEDURE HAS BEEN SUCCESSFULLY TESTED ON ALL DEVICES ABOVE,
> ABSOLUTELY NO GUARANTY CAN BE GIVEN THAT YOU WILL NOT DESTROY YOUR SWITCH,
> ANY OTHER EQUIPMENT INVOLVED OR HARM YOURSELF BY OPENING THE ELECTRONIC
> DEVICE. OPENING THE SWITCH WILL VOID ITS WARRANTY.

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
devices, set 8N1 @ 115200 baud and power up the switch.

The device will perform some examples and provide a minimal console, the
documentation of which can be found in the source code rtlplayground.c`.

## The command line
The command line is very rudimentary and mostly for testing purposes.
The following is a boot-log with some examples:
```
Detecting CPU
RTL8373 detected
Starting up...
  Flash controller

NIC reset
rtl8372_init called

RTL837X_REG_SDS_MODES: 0x00000bed

phy_config_8224 called

phy_config_8224 done

rtl8224_phy_enable called

rtl8224_phy_enable done
X
rtl8372_init done

A minimal prompt to explore the RTL8372:

CPU detected: RTL8373
Clock register: 0x00001101
Register 0x7b20/RTL837X_REG_SDS_MODES: 0x00000bed
Verifying PHY settings:

 Port   State   Link    TxGood          TxBad           RxGood          RxBad
1       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
2       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
3       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
4       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
5       On      2.5G    0x00000008      0x00000000      0x00000000      0x00000000
6       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
7       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
8       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
9       NO SFP  Down    0x00000000      0x00000000      0x00000000      0x00000000

> port 5 1g
  CMD: port 5 1g
PORT 04 1G

> stat
  CMD: stat
 Port   State   Link    TxGood          TxBad           RxGood          RxBad
1       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
2       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
3       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
4       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
5       On      1000M   0x00000035      0x00000000      0x00000017      0x00000000
6       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
7       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
8       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
9       NO SFP  Down    0x00000000      0x00000000      0x00000000      0x00000000

>
<SFP-RX OK>

<MODULE INSERTED>  Rate: 67  Encoding: 01
Lightron Inc.   WSPXG-ES3LC-IHA 0000


> stat
  CMD: stat
 Port   State   Link    TxGood          TxBad           RxGood          RxBad
1       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
2       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
3       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
4       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
5       On      1000M   0x00000065      0x00000000      0x0000003b      0x00000000
6       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
7       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
8       On      Down    0x00000000      0x00000000      0x00000000      0x00000000
9       SFP OK  10G     0x00000000      0x00000000      0x0000001c      0x00000000

> sfp
  CMD: sfp
Rate: 67  Encoding: 01
Lightron Inc.   WSPXG-ES3LC-IHA 0000

```
Enjoy playing!

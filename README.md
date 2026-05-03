# RTLPlayground
A Playground for Firmware development for advanced user of RTL8372/RTL8373 based 2.5GBit Switches.

For each hardware configuration of these devices, there is usually a managed and an
umanaged version sold, with mostly identical hardware. The aim is to provide management
features also for unmanaged devices with additional features such as Management VLAN,
dhcp servers, multi-language support, IPv6 and TLS-encrypted web-pages. At present, however
only the following features are provided:
- A modern web-interface with mouse-over to display further information
- A serial console interface to configure all features
- IGMP to configure Multicast streaming
- Port configuration showing detailed informtion about own and Link-partner advertised
  Speed settins and configuration of these settings on the local side
- Per-port configuration of frame sizes (MTUs) for Jumbo-Frame support or limiting MTUs
  for particular devices
- EEE (Energy Efficient Ethernet) can be configured per-port. Detailed information is
  provided for support offered by the link partner and the EEE status of a port.
- VLAN configuration
- SFP information is displayed on the inserted modules, the current sensor values such as
  temperatures, RX and TX power are displayed in the CLI and as mouse-over on the web
- Mirror configuration
- Link Aggregation Groups can be set up
- Detailed information on port packet statistics
- Configuration saved to flash via the web-interface
- Firmware updates via the web
- Installation as a firmware upgrade from the original web-interface

<img width="1420" height="623" alt="GUI" src="doc/images/gui.png" />

While the firmware provides already considerable improvements over the original managed firmware,
the firmware still lacks support for STP and the proprietary loop prevention
protocols as well as DHCP. If you need these features, do not install the playground on your managed
devices. In any case, installation is strongly discouraged unless you can at least make
a backup of the original flash content via a SOIC clamp such as also used for BIOS
backups and can re-install that firmware in case something is wrong. For this no soldering
skills are necessary.

The firmware supports all hardware featues of devices with
- 4 2.5GBit ports + 2 SFP+ ports
- 5 2.5GBIT + 1 SFP+ port
- 8 2.5GBit + 1 SFP+ port
Devices sold usually have a fairly common design, however there may be differences in the LED
configuration (switches have LEDs with different colours and use types of LEDs). The list
of tested devices can be found in [Supported devices](doc/supported_devices.md).

To do meaningful development you will need to use a serial console, so soldering skills
are required. Flashing must be done via a SOIC-8 PatchClamp or by soldering a socket
for the flash chip.

If you don't want to open your device, you can use the project's code to learn about the
devices by looking at the image using e.g. Ghidra. If you want to contribute to the
design of the web-interface or get a feeling for the interface first, a standalone
device simulator is provided, which runs entirely under Linux as a local webserver.

## Compiling Requirements

Install the following particular build requisites (Debian 12/13), note that Ubuntu 24.04
still has an older version of sdcc, but you will need sdcc version 4.5 for the code to compile:
```
sudo apt install make gcc sdcc xxd python-is-python3 libjson-c-dev
```
> [!CAUTION]
> DO NOT UPLOAD THE UPGADE IMAGE UNLESS YOU CAN MAKE A BACKUP USING A SOIC CLAMP OF THE ORIGINAL FIRMWARE.
> ERRORS IN THE FLASHING PROCEDURE CAN LOCK YOUR DEVICE AND THE ONLY WAY OUT IS A SOIC CLAMP !

## (1) Compiling for direct chip flashing AND upgrading an existing RTLPlayground running device

Edit machine.h with an editor like vi or nano. Select the correct machine the firmware should build for.
Now, building the firmware image should work:
```
make 
```
Note, that the image generated ends in .bin, not .img, in order to make IMSProg happy.

image location is stored in `RTLPlayground/output/rtlplayground.bin`

> [!CAUTION]
> This image can be flashed directly to the chip
> OR through the firmware update/upgrade interface of RTLPlaygound interface

## (2) Compiling for OEM running device with management options (web updrage)

Managed switches can be updated from the existing original firmware using an upgrade image.

You first need to build the firmware for direct chip flashing : See below

Then

```
cd installer
make 
```
image location is stored in  `RTLPlayground/installer/output/rtlplayground.bin`

> [!CAUTION]
> This image must ONLY be used for OEM original firmware web interface firmware upgrade.
> You do not need this image if you are already on RTLplayground firmware.

example of compilation console output

```
RTLPlayground/installer$ make
mkdir -p output/
gcc updatebuilder.c -o output/updatebuilder
sdas8051 -plosgff -o output/crtstart.rel crtstart.asm
sdcc -mmcs51 --code-loc 0x1000 -o output/installer.rel -c installer.c
sdcc -mmcs51 -Wl-bHOME=0x1100 -Wl-r -o output/rtlinstaller.ihx output/crtstart.rel output/installer.rel
cp ../output//rtlplayground.bin output/
./output//updatebuilder -i output/rtlinstaller.ihx output/rtlplayground.bin
Input file size: 524288
Bytes read: 524288
EOF
Payload sum 1 is: 0x29d10
Payload sum 2 is: 0x29d10
Payload sum with header is: 0x2b0fc
Payload sum is: 0xad8a75
Header checksum is: 0x4c3
```

## Installation
You can play with the image using ghidra or flash real Switch Hardware. For
ghidra see this information about [Ghidra images](ghidra.md).

> [!CAUTION]
> NOTE THAT WHILE THIS PROCEDURE HAS BEEN SUCCESSFULLY TESTED ON ALL DEVICES ABOVE,
> ABSOLUTELY NO GUARANTY CAN BE GIVEN THAT YOU WILL NOT DESTROY YOUR SWITCH,
> ANY OTHER EQUIPMENT INVOLVED OR HARM YOURSELF BY OPENING THE ELECTRONIC
> DEVICE. OPENING THE SWITCH WILL VOID ITS WARRANTY.

You can upload the upgrade image of managed switches via the web interface of the
original firmware just as if you were installing a firmware upgrade. However,
this is strongly discouraged, as you may brick your device, unless you can make
firmware backups via a SOIC clamp or soldered flash socket, first!

You can also flash the image directly to the chip. By the way direct flashing is the
only method to install RTLPlayground fot unmanaged devices.

> [!REMINDER]
> If your device already runs RTLPlayground, you must upload the binary file /RTLPlayground/output/rtlplayground.bin
> If your device is OEM, you must upload the binary file /RTLPlayground/installer/output/rtlplayground.bin
> If you flash the chip directly, you must use the binary file /RTLPlayground/output/rtlplayground.bin

## Flashing the ROM

You will need to open your switch to flash the image directly onto the flash chip,
which is done easiest using a SOIC-8 clip (alternatively you de-solder the
flash chip and install a SOIC adapter):
- Disconnect power from switch.
- Attach the clip onto the flash chip (Red line on Pin 1, Pin 1 has a point marker).
- Connect USB of flash programmer, the power LED on the switch will light up, check cabling if not.
- Don't panic, mixing up GND and 3.3V usually does not destroy the switch.
- Use IMSProg, Flashrom, or whatever Programmer to detect the chip.
- MAKE A BACKUP (DUMP) OF THE EXISTING FIRMWARE!
- Load the firmware into IMSProg
- Flash is to the ROM chip

## Connecting a serial interface (optional)

You can connect a serial cable to the UART port found on all the devices, 
set 8N1 @ 115200 baud.

## Power Up

When you power up the switch, the device will perform some examples and provide
a minimal console (if wired to a serial interface), the documentation of which can
be found in the source code rtlplayground.c`.

## The web-interface
The web-interface can be reached under the [default 192.168.10.247](http://192.168.10.247).
The default password is `1234`.

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

## Advanced settings

You can configure more deeply the switch without the need of the console mode.

While in compilation part, you might write directly to config.txt file before making the binary firmware

```
nano config.txt
```

If you want to modify settings after the flash is done, go to the Advanced Settings tab in System Settings

<img width="1085" height="646" alt="ADVANCED SETTINGS" src="doc/images/advanced_settings.png" />

```
ip xxx.xxx.xxx.xxx = IP adress of the switch
gw yyy.yyy.yyy.yyy = IP adress of the gateway
port x name xxx    = Name xxx the port number x
port z 1g          = Set 1g speed for port z
igmp on/off        = Turn IGMP on or off
```


Enjoy playing!

## Other documents
The following documents give further documentation on specific features of
the RTL837x SoCs:
- [RTL8372/3 Feature support](doc/hardware.md)
- [CPU Port](doc/CpuPort.md)
- [L2 learning](doc/l2.md) 
- [CPU Port](doc/CpuPort.md)
- [IGMP (IP-MC streaming)](doc/igmp.md)
- [SFP+ ports](doc/sfp.md) 
- [Trunking aka. port aggregation](doc/trunking.md)
- [VLAN](doc/vlan.md)
- [Modifications and Flash replacement](doc/mods.md)

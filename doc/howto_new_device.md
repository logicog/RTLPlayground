# Tool and Tip to Add a New Device.

# Advanced Tips & Ticks

## GPIO-Scanner Firmware

> [!CAUTION]
> Use at your own risk. This can damage your device and even worse your COMPUTER/LAPTOP.

To find GPIO pins on the board, you can use the [gpio-scanner firmware](https://github.com/vDorst/RTLPlayground/tree/gpio_scanner). It prints every second the GPIO status to the console, similarly to the `GPIO` command.
The firmware only initializes the UART pins, the rest is used as an input.

### Probing Circuit

![Probing Circuit](assets/probe-circuit.svg)

To make probing more easy, you can make this `probing circuit` on a breadboard, using a `100 Ohm` resistor, push-button, a voltmeter, and some wires.

Bottom wire have to connected to the ground of the switch. Top wire can be used to carefully probe the resistors/pins you believe to be GPIO pin, one at the time. Be careful not to short multiple pins. When you have picked a spot to probe, look at the voltmeter to see if the probed-signal is around 3.3 Volt. So you know that spot has a signal.
When pressing the button, the voltage should drop below 0.5 Volts. When it is below 0.5 Volts, check the console to see if a GPIO has changed. Repeat button press multiple times so you are sure which GPIO it is.
When the voltage is not dropping, you probably have a 3.3 volt supply.
Most useful places to find GPIO's are around connectors, LEDs, or buttons.

> [!NOTE]
> The SOC can also read-back the value on a pin, even when the pin is not configured as a GPIO input.
> So sometimes your see a GPIO change without touching a pin.
> As an example, UART-TX is one of the signals you will often see changing the pin value.

## Slave Port

The RTL837x has a Slave Port, this port is normally used to controller the chip with an external CPU, as can be found for example in a router or Wifi Accesspoint configuration.
The Slave Port supports 3 protocols, `I2C`, `SPI`, `SMI`, which are selected by the `strapping Pins`. Most `strapping Pins` are used as a LED output, after a reset the SOC senses a pull-up or pull-down resistor on these pins to determine the state of the setting. Many of these strapping pins are not easy to change, because the LED-circuit also would need to change.

>[!NOTE]
> Currently, this document only describes the `I2C` protocol.

### I2C-Host using a Raspberry Pi Pico RP2040

On Linux, the easiest way to create an 3.3V I2C-Host is with a standard Raspberry Pi Pico rp2040 with special firmware, suggested [here](https://github.com/logicog/RTLPlayground/issues/69#issuecomment-3704161277).
Firmware can be downloaded [here](https://github.com/dquadros/I2C-Pico-USB/).

#### Connect the I2C-Interface to the SOC

Many boards have an empty `SO8` spot on the PCB where an I2C-EEPROM can be placed.After reset, the SOC always tries to read the I2C-EEPROM, when that is done the I2C-bus turns into Slave Interface. A few boards also have a header with the Slave Port signals, for example the`SWTG024AS` and `2M-PCB23-V3_1` devices.

> [!CAUTION]
> Use at your own risk. This can damage your device and even worse your COMPUTER/LAPTOP.

|Signal | I2C-EEPROM pin | RP2040 with Firmware above |
| ----- | -------------- | ------ |
| DATA / SDA | 5 | GPIO6 |
| CLOCK / SCL | 6 |  GPIO7 |
| Ground / GND | 4 | PIN 8 |

When connected and powered-up, the `i2ctransfer` tool from the `i2c-tools` package, can be used to read/write from/to the SOC.

The SOC's i2c-address is `0x5c` (7-bit notation).

Reading out the chip ID can for example be done in the Linux shell 
via `i2ctransfer <I2C-BUS-NUMBER> w2@0x5c 0x00 0x04 r4`.
Note, that the value being returned is in 32-bit Little-endian.

For example; The I2C-bus number is `1`, so:
```bash
# i2ctransfer 1 w2@0x5c 0x00 0x04 r4
WARNING! This program can confuse your I2C bus, cause data loss and worse!
I will send the following messages to device file /dev/i2c-1:
msg 0: addr 0x5c, write, len 2, buf 0x00 0x04
msg 1: addr 0x5c, read, len 4
Continue? [y/N] y
0x00 0x00 0x72 0x83
```
Value of register `0x0004` is `0x00, 0x00, 0x72, 0x83`, so full 32-bit 
value is `0x83720000`.
So, my device is an RTL8372.

#### Dump script

With this script you can dump all the registers. It takes about 45 seconds to run.
This allows you for example to inspect the GPIO/LED settings from the original firmware.

*Note:* Some registers are redacted, because they are known to be unique to your device.

```bash
#!/bin/bash

# Check if i2ctransfer is installed
if ! command -v i2ctransfer &>/dev/null; then
    echo "i2ctransfer could not be found. Please install i2c-tools."
    exit 1
fi

# Check for the required argument
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <I2C_BUS>"
    echo "Example: $0 1"
    exit 1
fi

I2C_BUS=$1
DEVICE_ADDRESS=0x5C

redacted_registers=(16 20 24)
echo "--- DUMP START ---"
# Loop through the addresses from 0x0000 to 0xFFFF in steps of 4
for ADDRESS_16BIT in $(seq 0 4 65535); do
    # Redacted these number because they are known to be unique to your device.
    ADDRESS_HEX=$(printf "%04x" $ADDRESS_16BIT)
    if [[ " ${redacted_registers[@]} " =~ " $ADDRESS_16BIT " ]]; then
        echo "0x${ADDRESS_HEX}=<Redacted>"
        continue
    fi
    HIGH_BYTE="0x${ADDRESS_HEX:0:2}"
    LOW_BYTE="0x${ADDRESS_HEX:2:2}"

    # Execute i2ctransfer: write the two bytes and read 4 bytes
    VALUE=$(i2ctransfer -y $I2C_BUS w2@$DEVICE_ADDRESS $HIGH_BYTE $LOW_BYTE r4)

    if [ $? -eq 0 ]; then
        BYTE1=${VALUE:17:2}
        BYTE2=${VALUE:12:2}
        BYTE3=${VALUE:7:2}
        BYTE4=${VALUE:2:2}
        echo "0x${ADDRESS_HEX}=0x$BYTE1$BYTE2$BYTE3$BYTE4"
    else
        echo "Failed to read from address 0x$(printf "%04x" $ADDRESS_16BIT)"
        exit 1
    fi
done

echo "--- DUMP END ---"
exit 0
```

Output looks like this:

```
--- DUMP START ---
0x0000=0x00000000
0x0004=0x83737000
0x0008=0x00008000
0x000c=0x00300000
0x0010=<Redacted>
0x0014=<Redacted>
0x0018=<Redacted>
0x001c=0xcad0001c
0x0020=0xffff0000
...<snip>... 
0xfffc=0x00000000
--- DUMP END ---
```

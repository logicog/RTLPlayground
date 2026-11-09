# LED GPIO Utilities

This directory contains utilities to monitor and decode the GPIO and LED
configuration of an RTL837x switch. Scripts 1 to 3 talk to the IC over the I2C
bus through a dongle, script 4 reads the same registers over the switch's own
serial console instead.
(Only tested in Linux)

## Hardware requirements

To use the I2C utilities (scripts 1 to 3), you need:

  - A hardware dongle that can communicate with I2C devices. One example is the [I2C-Pico-USB](https://github.com/dquadros/I2C-Pico-USB) which provides USB-to-I2C connectivity.
  - Connection between the hardware dongle and the RTL837x's I2C communication port.

`dec_leds_from_rtl837x_uart.py` needs no dongle, only the USB-to-serial adapter
that is already used as the switch console.

## Scripts

### 1. `i2c_read_rtl_gpio.py`

This script reads RTL GPIO register values via I2C and displays changes in GPIO states.

**Features:**

  - Reads live RTL GPIO register values via I2C (default address 0x5C)
  - Monitors GPIO changes with delta detection
  - Allows specification of I2C bus, sleep interval, and ignored GPIO pins
  - Shows changes in real-time with GPIO index display

**Usage:**
```bash
# Basic usage (defaults to I2C bus 1, 2s sleep interval)
python3 i2c_read_rtl_gpio.py

# Specify I2C bus
python3 i2c_read_rtl_gpio.py --i2c-bus 0

# Specify sleep interval in seconds
python3 i2c_read_rtl_gpio.py --sleep-interval 5

# Ignore specific GPIO pins
python3 i2c_read_rtl_gpio.py --ignored-ios 28 31 34 44

# Combine options
python3 i2c_read_rtl_gpio.py --i2c-bus 2 --sleep-interval 1 --ignored-ios 28 31
```

**Output Format:**

  - Displays register address and data in hex format
  - Shows GPIO pins that have changed since last read
  - Example: `0044: 00 00 00 00 00 00 00 00`

### 2. `i2c_dump_rtl_regs.py`

This script dumps all register values from an RTL device via I2C.

**Features:**

  - Dumps registers sequentially from address 0x0000 to 0xFFFF
  - Reads 16 bytes at a time for efficiency
  - Configurable I2C bus number
  - Provides complete register dump in hex format

**Usage:**
```bash
# Basic usage (defaults to I2C bus 1)
python3 i2c_dump_rtl_regs.py >reg_dump.txt

# Specify I2C bus
python3 i2c_dump_rtl_regs.py --bus 0 >reg_dump.txt

# Or using short option
python3 i2c_dump_rtl_regs.py -b 2 >reg_dump.txt
```

**Output Format:**

  - Displays address and 16 bytes of data in hex format
  - Example: `0000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`

### 3. `dec_leds_from_dump.py`

This script decodes LED configuration from a register dump file ( the output from `i2c_dump_rtl_regs.py`).

**Features:**

  - Parses register dump files (reg_dump.txt format)
  - Decodes LED pad configurations and LED sets
  - Maps LED types to bit positions based on configuration
  - Outputs LED mux configuration and set mappings

**Usage:**
```bash
# Must be run in same directory as reg_dump.txt
python3 dec_leds_from_dump.py
```

**Output Format:**

  - LED pad configuration (hex values for each pad)
  - LED set configurations with descriptions of LED types
  - Port selection mappings

**Requirements:**

  - Requires a `reg_dump.txt` file containing the register dump output

### 4. `dec_leds_from_rtl837x_uart.py`

This script decodes the LED configuration of a live switch over its serial
console, using the firmware's own `regget` command - no I2C dongle and no
register dump file. It prints the same report as `dec_leds_from_dump.py` (the
LED pad mux, the LED sets and the set used by each port), followed by the pin
mux, so it is the quick way to read the wiring of a device that still runs its
stock firmware.

**Prerequisites:**

  - The console must be free: stop `minicom`/`screen` on that port first.
  - Some boards protect their console with a login/password prompt. The script
    does not know the password and cannot log in for you, so log in on the
    console first (enter the password in your terminal program) and only then
    run the script - on those boards it executes commands once the console
    session is authenticated. If the console is still waiting for a password, it
    stops as soon as the console answers with its login prompt instead of
    retrying, so a run does not turn into a series of failed login attempts.

**Features:**

  - Reads the LED registers with `regget` and prints `.led_mux`, the four LED
    sets and the LED set each port uses
  - Prints the decoded GPIO settings (LED or GPIO or special function pins)
  - Takes the port range from the chip model in register 0x0004: ports 0..8 on
    an RTL8373 (upper 16 bits `0x8373`), ports 3..8 on the RTL8372 boards;
    `--ports` overrides it
  - `--gpio` also dumps the GPIO output/input/direction registers, plus the
    list of pins set in each
  - Sends the address as `regget 0x65e0` (falling back to the plain form for
    firmwares whose parser rejects the `0x`) and only accepts an answer that
    names the register it asked for; the form that answered is used for the
    remaining reads
  - Paces the characters of a command and resends it when the console drops or
    mangles one, because the stock console loses input that arrives as one
    burst. Between attempts the console is resynchronised by draining it

**Usage:**
```bash
# Default console port (/dev/ttyUSB0) and baudrate (57600)
python3 dec_leds_from_rtl837x_uart.py

# Another port, plus the GPIO registers
python3 dec_leds_from_rtl837x_uart.py -p /dev/ttyUSB1 --gpio

# A console that copes with fast input, with a log of every exchange
python3 dec_leds_from_rtl837x_uart.py --char-delay 0 -v

# Decode only ports 0..5 instead of the range the chip model implies
python3 dec_leds_from_rtl837x_uart.py --ports 0 5
```

**Options:**

  - `-p` / `--port`: serial port (default: `/dev/ttyUSB0`)
  - `-b` / `--baudrate`: console baudrate (default: 57600)
  - `--cmd TEMPLATE`: register read command, `{addr}` is replaced by the hex
    address; repeatable and tried in order (default: `regget 0x{addr:04x}`,
    then `regget {addr:04x}`); a template without `{addr}` is rejected
  - `--char-delay`: seconds between the characters of a command, 0 to 1
    (default: 0.05, which measured clean on the stock console while 0.03 lost
    one command in eight)
  - `--ports MIN MAX`: port range resolved to LED sets, 0 to 63 (default: from
    the chip model, see above)
  - `--gpio`: also dump the GPIO registers and the pins set in them
  - `--timeout`: seconds to wait for an answer to a command (default: 5)
  - `--settle`: quiet seconds that end an answer without a prompt (default:
    0.4, must not exceed `--timeout`)
  - `--retries`: attempts per register (default: 4)
  - `--lenient`: also accept a bare hex value, for firmware that does not name
    the register in its answer (a mangled answer can then be misread)
  - `-v` / `--verbose`: print every console exchange, `-q` / `--quiet`: no
    progress output (this also suppresses the port-range line below)

> [!TIP]
> Unmanaged switches often run their console at 9600 baud and may use a
> different command and answer format, so for those try:
> ```bash
> python3 dec_leds_from_rtl837x_uart.py -b 9600 --cmd "rd 0x{addr:04x}" --lenient
> ```

**Output Format:**

  - Same report as `dec_leds_from_dump.py`: the LED pad table, the `.led_mux`
    line and the `.led_sets` initialiser, ready to paste into a machine
    definition, then one `Port <n>: SET <s>` line per decoded port
  - The pin mux table that follows it: `PIN_MUX_0/1/2` and `LED_GLB_IO_EN`, then
    one row per pin 0..63 with its field, field value and function. This is
    printed on every run, not only with `--gpio`
  - With `--gpio`, the six GPIO registers and the pins set in `GPIO_OUT`,
    `GPIO_IN` and `GPIO_OE`
  - The number of registers to read and an estimate of the time it takes are
    reported on stderr before the read (skipped with `-q`), then per-register
    progress, and the port range that was used, e.g.
    `Decoding ports 0..8 (from model 0x83737000)` or `(from --ports)`

## Requirements

The I2C scripts (1 to 3) require:

  - Python 3
  - `smbus2` Python package

`dec_leds_from_rtl837x_uart.py` requires:

  - Python 3
  - `pyserial`
  - an unlocked switch console: on boards that password-protect it, log in on
    the console before starting the script (see the prerequisites above)

(Depend upon Linux distribution) Install with:
```bash
apt install python3-smbus2 python3-serial
```
OR
```bash
pip3 install smbus2 pyserial
```


## Common Use Cases

`i2c_read_rtl_gpio.py`, `i2c_dump_rtl_regs.py` and `dec_leds_from_rtl837x_uart.py`
shall run with the **original** firmware, not RTLPlayground firmware: RTLPlayground
overwrites the LED registers with the machine definition of its build at boot, so
reading them there only shows the definition that is already flashed.


### Monitoring GPIO Changes
```bash
# Monitor GPIO changes on bus 1 with 1-second intervals
python3 i2c_read_rtl_gpio.py --i2c-bus 1 --sleep-interval 1
```

### Register Analysis
```bash
# Dump all device registers
python3 i2c_dump_rtl_regs.py --bus 2 > reg_dump.txt

# Analyze the register dump to understand LED configuration
python3 dec_leds_from_dump.py
```

### LED configuration without an I2C dongle
```bash
# Read the LED and GPIO configuration of the running (stock) firmware
python3 dec_leds_from_rtl837x_uart.py --gpio
```

## Configuration

The I2C scripts support command-line arguments for flexible configuration:

  - `--i2c-bus` or `-b`: Specify I2C bus (default: 1)
  - `--sleep-interval` or `-s`: Sleep between reads in seconds (default: 2)
  - `--ignored-ios` or `-i`: GPIO pins to ignore (default: [28, 31, 34, 44])

`dec_leds_from_rtl837x_uart.py` has its own options, listed in its section
above and in `--help`.

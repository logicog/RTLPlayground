# LED GPIO Utilities

This directory contains utility talking to RTL837x switch IC via I2C bus.
These scripts are designed to help with monitoring, debugging GPIO, and identifying LED configurations.
(Only tested in Linux)

## Hardware requirements

To use these utilities, you need:

  - A hardware dongle that can communicate with I2C devices. One example is the [I2C-Pico-USB](https://github.com/dquadros/I2C-Pico-USB) which provides USB-to-I2C connectivity.
  - Connection between the hardware dongle and the RTL837x's I2C communication port.

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

## Requirements

All scripts require:

  - Python 3
  - `smbus2` Python package

(Depend upon Linux distribution) Install with:
```bash
pip3 install python3-smbus2
```
OR
```bash
pip3 install smbus2
```


## Common Use Cases

Both `i2c_read_rtl_gpio.py` and `i2c_dump_rtl_regs.py` shall run with the **original** firmware, not RTLPlayground firmware.


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

## Configuration

All scripts support command-line arguments for flexible configuration:

  - `--i2c-bus` or `-b`: Specify I2C bus (default: 1)
  - `--sleep-interval` or `-s`: Sleep between reads in seconds (default: 2)
  - `--ignored-ios` or `-i`: GPIO pins to ignore (default: [28, 31, 34, 44])

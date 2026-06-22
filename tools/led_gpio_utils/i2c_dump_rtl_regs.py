#!/usr/bin/env python3

import argparse
from smbus2 import SMBus, i2c_msg

# Configuration
DEVICE_ADDR = 0x5C  # Replace with your device address
NUM_BYTES = 16  # Bytes to read

# Parse command line arguments
parser = argparse.ArgumentParser(description="Dump RTL registers via I2C")
parser.add_argument(
    "-b",
    "--i2c-bus",
    type=int,
    required=True,
    help="I2C bus number (use i2cdetect -l to find out the bus number of the dongle)",
)
args = parser.parse_args()

# Open I2C bus
with SMBus(args.i2c_bus) as bus:

    for addr in range(0, 65536, NUM_BYTES):
        # Create write message (send register address)
        write = i2c_msg.write(DEVICE_ADDR, [addr >> 8, addr & 0xFF])

        # Create read message (read 2 bytes)
        read = i2c_msg.read(DEVICE_ADDR, NUM_BYTES)

        # Perform combined transaction
        bus.i2c_rdwr(write, read)

        new_data = list(read)

        # Convert read message to string
        datastr = " ".join([f"{x:02x}" for x in new_data])
        print(f"{addr:04x}: {datastr}")

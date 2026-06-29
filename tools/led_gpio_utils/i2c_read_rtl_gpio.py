#!/usr/bin/env python3

import time
import argparse
from smbus2 import SMBus, i2c_msg

# Configuration
I2C_BUS = 1
DEVICE_ADDR = 0x5C  # Replace with your device address
NUM_BYTES = 8  # Bytes to read
SLEEP_INTERVAL = 1

# Handle command line arguments for ignored IOs
# default to ignore IOs related to SYS_LED, UART, SMI, SPI
IGNORED_IOS = [28, 31, 32, 34, 35, 42, 43, 44, 45]

# Setup argument parser
parser = argparse.ArgumentParser(description="Read I2C data from RTL GPIO expander")
parser.add_argument(
    "-b",
    "--i2c-bus",
    type=int,
    required=True,
    help="I2C bus number (use i2cdetect -l to find out the bus number of the dongle)",
)
parser.add_argument(
    "-s",
    "--sleep-interval",
    type=int,
    default=SLEEP_INTERVAL,
    help=f"Sleep interval in seconds (default: {SLEEP_INTERVAL})",
)
parser.add_argument(
    "-i",
    "--ignored-ios",
    nargs="*",
    type=int,
    default=IGNORED_IOS,
    help=f"List of GPIO pins to ignore (default: {IGNORED_IOS})",
)
args = parser.parse_args()
I2C_BUS = args.i2c_bus

first_read = True
last_data = [0 for x in range(NUM_BYTES)]

# Open I2C bus
with SMBus(I2C_BUS) as bus:
    addr = 0x44

    while True:
        # Create write message (send register address)
        write = i2c_msg.write(DEVICE_ADDR, [addr >> 8, addr & 0xFF])

        # Create read message (read NUM_BYTES bytes)
        read = i2c_msg.read(DEVICE_ADDR, NUM_BYTES)

        # Perform combined transaction
        bus.i2c_rdwr(write, read)

        new_data = list(read)
        if first_read:
            delta_data = last_data
            first_read = False
        else:
            delta_data = [new_data[x] ^ last_data[x] for x in range(NUM_BYTES)]
        last_data = new_data

        # Convert read message to list
        datastr = " ".join([f"{x:02x}" for x in new_data])
        deltastr = ""
        chg_list = []
        for x in range(NUM_BYTES):
            if delta_data[x] == 0:
                continue
            for y in range(8):
                if delta_data[x] & (1 << y):
                    idx = x * 8 + y
                    if idx not in args.ignored_ios:
                        deltastr += f"\n{'  ':8s}GPIO{idx}"
        # deltastr = " ".join([ f"{x:02x}" for x in delta_data])
        print(f"{addr:04x}: {datastr}{deltastr}")
        time.sleep(args.sleep_interval)

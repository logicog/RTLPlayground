#!/usr/bin/env python3

import time
import sys
import argparse
from smbus2 import SMBus, i2c_msg

# Configuration
I2C_BUS = 1
DEVICE_ADDR = 0x5C  # Replace with your device address
NUM_BYTES = 8       # Bytes to read
SLEEP_INTERVAL = 2

# Handle command line arguments for ignored IOs
IGNORED_IOS = [ 28, 31, 34, 44]

# Setup argument parser
parser = argparse.ArgumentParser(description='Read I2C data from RTL GPIO expander')
parser.add_argument('--i2c-bus', '-b', type=int, default=1,
                    help='I2C bus number (default: 1)')
parser.add_argument('--sleep-interval', '-s', type=int, default=2,
                    help='Sleep interval in seconds (default: 2)')
parser.add_argument('--ignored-ios', '-i', nargs='*', type=int, default=[28, 31, 34, 44],
                    help='List of GPIO pins to ignore (default: [28, 31, 34, 44])')
args = parser.parse_args()
I2C_BUS = args.i2c_bus
SLEEP_INTERVAL = args.sleep_interval
IGNORED_IOS = args.ignored_ios

first_read = True
last_data = [ 0 for x in range(NUM_BYTES) ]

# Open I2C bus
with SMBus(I2C_BUS) as bus:
    addr = 0x44

    while True:
        # Create write message (send register address)
        write = i2c_msg.write(DEVICE_ADDR, [addr>>8, addr & 0xff])
    
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
        datastr = " ".join([ f"{x:02x}" for x in new_data])
        deltastr = ""
        chg_list = []
        for x in range(NUM_BYTES):
            if delta_data[x] == 0:
                continue
            for y in range(8):
                if delta_data[x] & ( 1<<y):
                    idx = x*8 + y
                    if idx not in IGNORED_IOS:
                        deltastr += f"\n{'  ':8s}GPIO{idx}"
        # deltastr = " ".join([ f"{x:02x}" for x in delta_data])
        print(f"{addr:04x}: {datastr}{deltastr}")
        time.sleep(SLEEP_INTERVAL)
#!/usr/bin/env python3

import time
from smbus2 import SMBus, i2c_msg

# Configuration
I2C_BUS = 1
DEVICE_ADDR = 0x5C  # Replace with your device address
NUM_BYTES = 16       # Bytes to read

# Open I2C bus
with SMBus(I2C_BUS) as bus:

	for addr in range (0, 65536, NUM_BYTES):
		# Create write message (send register address)
		write = i2c_msg.write(DEVICE_ADDR, [addr>>8, addr & 0xff])
	
		# Create read message (read 2 bytes)
		read = i2c_msg.read(DEVICE_ADDR, NUM_BYTES)
	
		# Perform combined transaction
		bus.i2c_rdwr(write, read)
	
		new_data = list(read)

		# Convert read message to string
		datastr = " ".join([ f"{x:02x}" for x in new_data])
		print(f"{addr:04x}: {datastr}")

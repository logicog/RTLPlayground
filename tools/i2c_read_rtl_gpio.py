#!/usr/bin/env python3

import time
from smbus2 import SMBus, i2c_msg

# Configuration
I2C_BUS = 1
DEVICE_ADDR = 0x5C  # Replace with your device address
NUM_BYTES = 8       # Bytes to read
SLEEP_INTERVAL = 2
IGNORED_IOS = [ 28, 31, 34, 44]

first_read = True
last_data = [ 0 for x in range(NUM_BYTES) ]
# Open I2C bus
with SMBus(I2C_BUS) as bus:

	addr = 0x44

	while True:
		# Create write message (send register address)
		write = i2c_msg.write(DEVICE_ADDR, [addr>>8, addr & 0xff])
	
		# Create read message (read 2 bytes)
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

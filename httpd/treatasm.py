#!/usr/bin/python
import re
import sys

isBanked = False

regex_bank = re.compile(r'^\s*\.area\s+BANK\d+\s*\(CODE\)')
regex_const = re.compile(r'^\s*\.area\s+CONST\s*\(CODE\)')
regex_area = re.compile(r'^\s*\.area\s+\(CODE\)')

with open(sys.argv[1], "r") as file:
	for line in file:
		line = line.rstrip()
		bmatch = re.search(regex_bank, line)
		if bmatch:
			isBanked = True
			print(line)
		else:
			cmatch = re.search(regex_const, line)
			if not isBanked or not cmatch:
				print(line)
			amatch = re.search(regex_area, line)
			if amatch:
				isBanked = False
				print(line)

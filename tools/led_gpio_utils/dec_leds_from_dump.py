#!/usr/bin/env python3

regs = list(range(65536))

glb_mux = 0x65E0

led3_0_set1 = 0x6528
led1_0_set0 = 0x6548
led_port_sel = 0x654C


def get_word(i):
    val = (regs[i + 3] << 24) + (regs[i + 2] << 16) + (regs[i + 1] << 8) + regs[i]
    # 	print(f"READING {i:04x} : {val:08x}")
    return val


with open("reg_dump.txt", "r") as file:
    for line in file:
        addr, data = line.split(":")
        data = [int(x, 16) for x in data.strip().split(" ")]
        addr = int(addr, 16)
        for x in range(len(data)):
            regs[addr + x] = data[x]

ledstr = []
ledfmt = []
print("LED pad Configuration:")
for i in range(28):
    j = i % 5
    if j == 0:
        idx = glb_mux + (i // 5) * 4
        val = get_word(idx)
    ledval = (val >> (6 * j)) & 0x3F
    ledstr.append(f"{ledval:02x}")
    ledfmt.append(f"{i:02x}")
print(f"{' '.join(ledfmt)}")
print(f"{' '.join(ledstr)}")
print(f".led_mux = {{ 0x{', 0x'.join(ledstr)} }},")

LED_TYPES = [
    " 10G",
    16,
    " TWO_5G",
    17,
    " 5G",
    18,
    " TWO_2G5",
    19,
    " 2G5",
    0,
    " TWO_1G",
    1,
    " 1G",
    2,
    " 500M",
    3,
    " 100M",
    4,
    " 10M",
    5,
    " LINK",
    6,
    " LINK_FLASH",
    7,
    " ACT",
    8,
    " RX",
    9,
    " TX",
    10,
    " COL",
    11,
    " DUPLEX",
    12,
    " TRAINING",
    13,
    " MASTER",
    14,
    "",
    15,
]

led_set = []
led_set_str = []
print("\nLED-set Configuration:")
print("LED-ID         0               1               2               3")
for i in range(4):
    idval = []
    idvalstr = []
    for id in range(4):
        val = 0xFFFF & (
            get_word(led1_0_set0 - 8 * i - ((id >> 1) << 2)) >> (16 * (id & 1))
        )
        valhi = (
            0xF & (get_word(led3_0_set1 - 4 * (i >> 1)) >> (16 * (i & 1) + 4 * id))
        ) << 16
        val += valhi
        idval.append(val)
        valstr = "("
        for bit in range(0, len(LED_TYPES), 2):
            if val & (1 << (LED_TYPES[bit + 1])):
                valstr += LED_TYPES[bit]
        valstr += ")"
        idvalstr.append(valstr)
    led_set.append(idval)
    led_set_str.append(idvalstr)
    idstr = "           ".join([f"{d:05x}" for d in idval])
    print(f"SET {i}:     {idstr}")
    # print(f"{idvalstr}")

portsel = get_word(led_port_sel)
for i in range(3, 9):
    sel = (portsel >> (i << 1)) & 3
    print(f"Port {i}: SET {sel}: {', '.join(led_set_str[sel])}")

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

isRTL8373 = regs[0x6] == 0x73
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
    "LEDS_10G",
    " TWO_5G",
    17,
    "LEDS_TWO_PAIR_5G",
    " 5G",
    18,
    "LEDS_5G",
    " TWO_2G5",
    19,
    "LEDS_TWO_PAIR_2G5",
    " 2G5",
    0,
    "LEDS_2G5",
    " TWO_1G",
    1,
    "LEDS_TWO_PAIR_1G",
    " 1G",
    2,
    "LEDS_1G",
    " 500M",
    3,
    "LEDS_500M",
    " 100M",
    4,
    "LEDS_100M",
    " 10M",
    5,
    "LEDS_10M",
    " LINK",
    6,
    "LEDS_LINK",
    " LINK_FLASH",
    7,
    "LEDS_LINK_FLASH",
    " ACT",
    8,
    "LEDS_ACT",
    " RX",
    9,
    "LEDS_RX",
    " TX",
    10,
    "LEDS_TX",
    " COL",
    11,
    "LEDS_COL",
    " DUPLEX",
    12,
    "LEDS_DUPLEX",
    " TRAINING",
    13,
    "LEDS_TRAINING",
    " MASTER",
    14,
    "LEDS_MASTER",
    "",
    15,
    "",
]

led_set = []
led_set_str = []
led_set_def = []
print("\nLED-set Configuration:")
print("LED-ID         0               1               2               3")
for i in range(4):
    idval = []
    idvalstr = []
    defvalstr = []
    for id in range(4):
        defstr = []
        val = 0xFFFF & (
            get_word(led1_0_set0 - 8 * i - ((id >> 1) << 2)) >> (16 * (id & 1))
        )
        valhi = (
            0xF & (get_word(led3_0_set1 - 4 * (i >> 1)) >> (16 * (i & 1) + 4 * id))
        ) << 16
        val += valhi
        idval.append(val)
        valstr = "("
        for bit in range(0, len(LED_TYPES), 3):
            if val & (1 << (LED_TYPES[bit + 1])):
                valstr += LED_TYPES[bit]
                defstr.append(LED_TYPES[bit + 2])
        valstr += ")"
        idvalstr.append(valstr)
        tmpdefstr = " | ".join(defstr)
        if not tmpdefstr :
            tmpdefstr = "0"
        defvalstr.append(tmpdefstr)
    led_set_def.append("{ " + ",\n\t\t\t".join(defvalstr) + "\n\t\t      }")

    led_set.append(idval)
    led_set_str.append(idvalstr)
    idstr = "           ".join([f"{d:05x}" for d in idval])
    print(f"SET {i}:     {idstr}")
    # print(f"{idvalstr}")

portsel = get_word(led_port_sel)
startport = 0 if isRTL8373 else 3
for i in range(startport, 9):
    sel = (portsel >> (i << 1)) & 3
    print(f"Port {i}: SET {sel}: {', '.join(led_set_str[sel])}")

print("\n\n\t.led_sets = { " + ",\n\t\t      ".join(led_set_def) + "\n\t\t},")

# Pin mux decoding, see doc/gpio.md and rtl837x_pins.c.
pin_mux_0 = 0x7F8C
pin_mux_1 = 0x7F90
pin_mux_2 = 0x7F94
pin_led_glb_io_en = 0x65DC

# Pads 0-29 are the LED pads: 0 leaves the pad to the LED engine, 1 makes it a GPIO.
led_pad_functions = {pin: f"LED{pin}" for pin in range(28)}
led_pad_functions[28] = "SYS_LED"
led_pad_functions[29] = "GLB_RLDP_LED_EN"

# PIN_MUX_1, one bit per pin: (bit, function name, bit value that selects it).
# GPIO31/UART0_TX is the exception: the stock firmware has bit 0 set, and no
# console would come out of the pad if a 1 meant GPIO there.
mux1_bits = {
    31: (0, "UART0_TX", 1),
    32: (0, "UART0_RX", 1),
    33: (2, "GPIO_INT", 0),
    34: (3, "MDC0", 0),
    35: (4, "MDIO0", 0),
    55: (19, "PTP_CLK125M_IN", 0),
    56: (20, "PTP_CLK_OUT", 0),
    57: (21, "PTP_TOD_OUT", 0),
    58: (22, "PTP_PPS_OUT", 0),
    59: (23, "PTP_TOD_IN", 0),
    60: (24, "PTP_PPS_IN", 0),
    61: (27, "SYNCELOCK0", 0),
    62: (28, "SYNCELOCK1", 0),
    39: (29, "I2C_SDA4", 0),
    36: (30, "PWM_OUT", 0),
}

# PIN_MUX_1, two bits per pin: (low bit, {field value: function}).
# GPIO40/41 share one field; 0b00 is GPIO on all of these.
mux1_fields = {
    40: (5, {0: "GPIO", 1: "MDC1", 2: "I2C_SCL3"}),
    41: (5, {0: "GPIO", 1: "MDIO1", 2: "I2C_SDA3"}),
    46: (7, {0: "GPIO", 1: "I2C_SCL0"}),
    47: (9, {0: "GPIO", 1: "I2C_SDA0"}),
    48: (11, {0: "GPIO", 1: "I2C_SCL1"}),
    49: (13, {0: "GPIO", 1: "I2C_SDA1"}),
    50: (15, {0: "GPIO", 1: "I2C_SCL2"}),
    51: (17, {0: "GPIO", 1: "I2C_SDA2"}),
}

# PIN_MUX_2, one bit per pin: 0 = GPIO, 1 = the pin's ACL function.
mux2_bits = {
    52: (0, "ACL_BIT0_EN"),
    53: (1, "ACL_BIT1_EN"),
    54: (2, "ACL_BIT2_EN"),
    30: (3, "ACL_BIT3_EN"),
}

# Pins with no mux field.
fixed_pins = {
    37: "GPIO (no mux field)",
    38: "GPIO (no mux field)",
    42: "SPI flash (no mux field)",
    43: "SPI flash (no mux field)",
    44: "SPI flash (no mux field)",
    45: "SPI flash (no mux field)",
    63: "no mux field of its own",
}


def pin_setting(pin, mux0, mux1, mux2):
    """Field, field value and function of one pin, from the three mux registers."""
    if pin < 30:
        bit = (mux0 >> pin) & 1
        return f"PIN_MUX_0[{pin}]", f"{bit}", "GPIO" if bit else led_pad_functions[pin]

    if pin in mux2_bits:
        bit, name = mux2_bits[pin]
        val = (mux2 >> bit) & 1
        return f"PIN_MUX_2[{bit}]", f"{val}", name if val else "GPIO"

    if pin in mux1_bits:
        bit, name, function_level = mux1_bits[pin]
        val = (mux1 >> bit) & 1
        return f"PIN_MUX_1[{bit}]", f"{val}", name if val == function_level else "GPIO"

    if pin in mux1_fields:
        bit, functions = mux1_fields[pin]
        val = (mux1 >> bit) & 3
        return f"PIN_MUX_1[{bit + 1}:{bit}]", f"0b{val:02b}", functions.get(val, "undecoded")

    return "-", "-", fixed_pins.get(pin, "no mux field")


# Pads 27-29 are the "high LEDs": LED_27, the system LED (LED_28_SYS) and the
# RLDP LED (LED_29). Unlike pads 0-27 they have no field in LED_GLB_MUX_1..6;
# their pin function is bit 27-29 of PIN_MUX_0 (0 = LED, 1 = GPIO) and their
# driver enable is bit 27-29 of LED_GLB_IO_EN (1 = on), see doc/leds.md.
high_leds = [(27, "LED_27"), (28, "LED_28_SYS"), (29, "LED_29")]

mux0 = get_word(pin_mux_0)
mux1 = get_word(pin_mux_1)
mux2 = get_word(pin_mux_2)
glb_io_en = get_word(pin_led_glb_io_en)
print("\nPin MUX Configuration:")
print(f"PIN_MUX_0 (0x{pin_mux_0:04x}) = 0x{mux0:08x}   pads 0-29: 0 = LED function, 1 = GPIO")
print(f"PIN_MUX_1 (0x{pin_mux_1:04x}) = 0x{mux1:08x}   one or two bits per pin, see below")
print(f"PIN_MUX_2 (0x{pin_mux_2:04x}) = 0x{mux2:08x}   pins 30, 52-54: 0 = GPIO, 1 = ACL function")
print(f"PIN_GLB_IO_EN (0x{pin_led_glb_io_en:04x}) = 0x{glb_io_en:08x}")
print()
print(f"{'Pin':<9}{'Field':<18}{'Value':<9}Function")
for pin in range(64):
    field, val, function = pin_setting(pin, mux0, mux1, mux2)
    print(f"GPIO{pin:<5}{field:<18}{val:<9}{function}")


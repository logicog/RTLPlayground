#!/usr/bin/env python3
"""Decode the LED wiring of a live RTL837x switch over its serial console.

This script talks to the switch's serial console and reads the LED registers with
the firmware's own ``regget`` command. The report it prints is the same as
``dec_leds_from_dump.py``: the LED pad mux (``.led_mux``), the four LED sets
(``.led_sets``) and the set each port uses (``.port_led_set``) - exactly what a
``machine/<NAME>.c`` definition needs - followed by the pin mux, i.e. the
function every GPIO pin is set to.

Run it against the firmware that still holds the wiring you want to learn, i.e.
the *stock* firmware of a new board: RTLPlayground overwrites the LED registers
with its machine definition during boot, so reading them there only shows the
definition that is already flashed.

Notes on the console:

  * The address is sent with a ``0x`` prefix (``regget 0x65e0``), which is what
    the stock firmware expects. RTLPlayground's ``atoi_hex()`` rejects the
    ``x``, so the plain form (``regget 65e0``) is tried as well - see
    ``--cmd``; whichever form a firmware accepts is used for all later reads.
  * The stock firmware drops characters when a command arrives as one burst, so
    the characters are paced (``--char-delay``, 50 ms by default; on this
    console 30 ms still lost one command in eight). A command whose echo does
    not match what was sent is sent again.
  * It answers with ``reg 0x65e0,data 0x8144040``; the leading zeros are
    trimmed, other firmwares print ``VAL:`` or a bare hex value. All are
    accepted.
  * The port range is taken from the chip model in register 0x0004: an RTL8373
    (upper 16 bits 0x8373) serves ports 0..8, an RTL8372 only 3..8. ``--ports``
    overrides it.
  * The console must be free - stop minicom/screen on the port first.
  * Some boards protect their console with a login/password prompt. The script
    cannot log in for you, so log in on the console first and run it afterwards;
    a console that is still waiting for a password is reported after the first
    command instead of being fed a whole run of them.

Requirements: Python 3, pyserial (``apt install python3-serial``).
"""

import argparse
import re
import sys
import time

try:
    import serial
except ImportError:  # pragma: no cover - depends on the host
    sys.exit("pyserial is missing, install it with: apt install python3-serial")

# Register addresses, named as in rtl837x_regs.h.
RTL837X_REG_MODEL_NAME_INFO = 0x0004  # upper 16 bits: the chip model
RTL837X_REG_LED_GLB_MUX_1 = 0x65E0  # pads 0..27, five 6-bit fields per word
RTL837X_REG_LED3_0_SET1 = 0x6528    # LED type, upper nibbles of sets 0/1
RTL837X_REG_LED1_0_SET0 = 0x6548    # LED type, lower 16 bits of a set
RTL837X_LED_PORT_SET_SEL = 0x654C   # 2 bits per port selecting the LED set
RTL837X_REG_LED_GLB_IO_EN = 0x65DC  # globals for the LED pads
RTL837X_PIN_MUX_0 = 0x7F8C          # pads 0..29: 0 = LED function, 1 = GPIO
RTL837X_PIN_MUX_1 = 0x7F90          # one or two bits per pin, see below
RTL837X_PIN_MUX_2 = 0x7F94          # pins 30, 52..54: 0 = GPIO, 1 = ACL function

# Upper 16 bits of MODEL_NAME_INFO. An RTL8373 serves ports 0..8, the boards
# built around an RTL8372 only 3..8.
RTL8373_MODEL_ID = 0x8373

# Optional GPIO survey, named as in rtl837x_regs.h.
GPIO_REGS = [
    ("GPIO_OUT0", 0x003C),
    ("GPIO_OUT1", 0x0040),
    ("GPIO_IN0", 0x0044),
    ("GPIO_IN1", 0x0048),
    ("GPIO_OE0", 0x004C),
    ("GPIO_OE1", 0x0050),
]

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

# The pin mux tables below decode PIN_MUX_0/1/2, see doc/gpio.md and
# rtl837x_pins.c.

# Pads 0-29 are the LED pads: 0 leaves the pad to the LED engine, 1 makes it a
# GPIO.
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


# What an answer to a register read looks like. Stock firmware:
# "reg 0x65e0,data 0x8144040", RTLPlayground: "REGGET: 0x65e0: VAL: 0x0000ffff".
# Both name the register they answer for, and that is what makes a read
# trustworthy: this console drops the characters of the command, and the rest of
# the line is executed on its own ("regget x65e4" answers "reg 0x0,data 0x0"),
# so an answer is only used when it names the address that was asked for.
ANSWER_PATTERNS = [
    re.compile(r"\breg\s+0x([0-9a-fA-F]{1,4})\s*,\s*data\s+(?:0x)?([0-9a-fA-F]{1,8})\b"),
    # RTLPlayground prints both the register and the value with a 0x prefix:
    # "REGGET: 0x7f8c: VAL: 0x28000000".
    re.compile(
        r"\bREGGET:\s*(?:0x)?([0-9a-fA-F]{1,4})\s*:\s*"
        r"VAL:\s*(?:0x)?([0-9a-fA-F]{1,8})\b"
    ),
]

# Answers of firmware that names only the value. With --lenient these are
# accepted, at the risk of reading a value out of a mangled answer.
LENIENT_PATTERNS = [
    re.compile(r"0x([0-9a-fA-F]{1,8})\b"),
    re.compile(r"\b([0-9a-fA-F]{8})\b"),
]

# A console that is waiting for a login or a password instead of running
# commands. Some boards protect their serial console this way, and the script
# cannot log in for you: every command sent into the prompt is one failed login
# attempt, so it stops as soon as one is recognised.
LOGIN_PATTERN = re.compile(
    r"\b(?:password|passwd|login|username|user\s?name|user)\b[^\S\r\n]*[:>]\s*$"
    r"|\b(?:password|login)\s+(?:incorrect|invalid|failed|error|wrong)\b"
    r"|\baccess\s+denied\b"
    r"|\bauthentication\s+failed\b",
    re.I | re.M,
)

# A console that did not understand (or only partly received) the command.
REJECTED_PATTERN = re.compile(r"unsupport|unknown command|syntax error", re.I)

# The answer starts with an echo of the command, unless it is one of the known
# answer forms (a firmware may not echo at all).
ANSWER_START_PATTERN = re.compile(r"^\s*(reg\s+0x|REGGET:)", re.I)

# A console prompt: the last line ends in ':' (stock firmware: "RTL8373:") or
# one of the usual shell characters. Only used to stop reading earlier.
PROMPT_PATTERN = re.compile(r"([^\r\n]*[:>#\$])[ \t]*$")


class ConsoleError(Exception):
    """The console could not be used or did not answer with a register value."""


def login_prompt(text):
    """True if the console is asking for a login/password, not for commands."""
    return bool(LOGIN_PATTERN.search(text))


def model_ports(model_info):
    """The port range the chip reports: RTL8373 has ports 0..8, RTL8372 3..8."""
    return (0, 8) if (model_info >> 16) == RTL8373_MODEL_ID else (3, 8)


def register_addresses(with_gpio):
    """The registers the report needs, each only once and in a stable order."""
    addrs = [RTL837X_REG_MODEL_NAME_INFO]
    addrs += [RTL837X_REG_LED_GLB_MUX_1 + (i // 5) * 4 for i in range(28)]
    addrs += [
        RTL837X_REG_LED1_0_SET0 - 8 * i - ((id >> 1) << 2)
        for i in range(4)
        for id in range(4)
    ]
    addrs += [RTL837X_REG_LED3_0_SET1 - 4 * (i >> 1) for i in range(4)]
    addrs.append(RTL837X_LED_PORT_SET_SEL)
    addrs += [
        RTL837X_PIN_MUX_0,
        RTL837X_PIN_MUX_1,
        RTL837X_PIN_MUX_2,
        RTL837X_REG_LED_GLB_IO_EN,
    ]
    if with_gpio:
        addrs += [addr for _, addr in GPIO_REGS]
    return sorted(set(addrs))


def parse_register_value(answer, addr, cmd, lenient=False):
    """The value a console answer gives for ``addr``, or None if it has none.

    The echoed command is dropped first: it carries the register address, which
    a pattern could mistake for the value.
    """
    body = "\n".join(
        line for line in answer.splitlines() if line.strip() != cmd.strip()
    )
    if not body.strip() or REJECTED_PATTERN.search(body):
        return None
    for pattern in ANSWER_PATTERNS:
        for reg, value in pattern.findall(body):
            if int(reg, 16) == addr:
                return int(value, 16) & 0xFFFFFFFF
    if lenient:
        # A value without a register number: the address must not be read as
        # one, so take it out of the text first.
        text = re.sub(
            rf"0x0*{addr:x}\b|\b0*{addr:x}\b", "", body, flags=re.IGNORECASE
        )
        for pattern in LENIENT_PATTERNS:
            matches = pattern.findall(text)
            if matches:
                return int(matches[-1], 16) & 0xFFFFFFFF
    return None


def echo_mismatch(answer, cmd):
    """True if the console echoed a different command than the one sent.

    That is what a dropped input character looks like, and resending right away
    is cheaper and safer than reading from a line that was cut in two.
    """
    first = answer.strip().splitlines()[0].strip() if answer.strip() else ""
    if not first or ANSWER_START_PATTERN.match(first) or first == cmd.strip():
        return False
    # Anything else that looks echo-like but differs means lost characters.
    return first[:4].lower() in cmd.strip()[:4].lower()


class RtlConsole:
    """Line-oriented client for the switch's serial console."""

    def __init__(self, port, baud, cmds, char_delay, timeout, settle, retries, verbose, lenient):
        self.cmds = list(cmds)
        self.char_delay = char_delay
        self.timeout = timeout
        self.settle = settle
        self.retries = retries
        self.verbose = verbose
        self.lenient = lenient
        try:
            self.ser = serial.Serial(port, baud, timeout=0.1, write_timeout=2)
        except serial.SerialException as exc:
            raise ConsoleError(
                f"cannot open {port}: {exc} (is minicom or screen using it?)"
            ) from exc

    def close(self):
        self.ser.close()

    def drain(self):
        """Discard what is still buffered and return it, for the verbose log."""
        pending = self.ser.read(65536).decode("ascii", "replace")
        self.ser.write(b"\r")
        time.sleep(0.2)
        pending += self._read_answer()
        return pending

    def _write_paced(self, text):
        for char in text:
            self.ser.write(char.encode("ascii"))
            self.ser.flush()
            if self.char_delay:
                time.sleep(self.char_delay)

    def _read_answer(self):
        """Read until the prompt, a quiet period or the timeout ends the answer."""
        deadline = time.monotonic() + self.timeout
        last_data = time.monotonic()
        buf = b""
        while time.monotonic() < deadline:
            chunk = self.ser.read(4096)
            if chunk:
                buf += chunk
                last_data = time.monotonic()
                tail = buf.split(b"\n")[-1].decode("ascii", "replace")
                if b"\n" in buf and PROMPT_PATTERN.search(tail):
                    break
            elif buf and time.monotonic() - last_data >= self.settle:
                break
        return buf.decode("ascii", "replace")

    def send(self, cmd):
        """Send one command and return the console's answer as text."""
        self.ser.reset_input_buffer()
        self._write_paced(cmd)
        self.ser.write(b"\r")
        self.ser.flush()
        answer = self._read_answer()
        if self.verbose:
            print(f"--- {cmd}\n{answer.strip()}", file=sys.stderr)
        return answer

    def regget(self, addr):
        """Read one 32 bit register, trying the command forms of ``--cmd``.

        Both a console that was busy (the command was dropped) and one that does
        not know this form of the command are handled by retrying and, once the
        attempts are used up, moving on to the next form.
        """
        answer = ""
        rejected = []
        for template in list(self.cmds):
            cmd = template.format(addr=addr)
            for attempt in range(self.retries):
                answer = self.send(cmd)
                if echo_mismatch(answer, cmd):
                    # The console lost characters of the command, so the answer
                    # is about some other register: send the command again.
                    self.drain()
                    continue
                value = parse_register_value(answer, addr, cmd, self.lenient)
                if value is not None:
                    # Use the form this firmware accepts from now on.
                    self.cmds.remove(template)
                    self.cmds.insert(0, template)
                    return value
                if login_prompt(answer):
                    # Nothing is executed until the password is entered, so
                    # every further attempt is another failed login.
                    raise ConsoleError(
                        "the console is waiting for a login/password "
                        f"({answer.strip()!r}): log in on the console first, "
                        "then run this script again"
                    )
                if attempt < self.retries - 1:
                    # The line was cut in two and the rest executed on its own,
                    # so clear the console before sending the command again.
                    if self.verbose:
                        print("--- no value, resynchronising", file=sys.stderr)
                    self.drain()
            rejected.append(cmd)
        raise ConsoleError(
            "no register value, tried "
            + ", ".join(repr(cmd) for cmd in rejected)
            + f", last answer: {answer!r}"
        )


def read_registers(console, addresses, quiet):
    """Read every address with regget and return {address: value}."""
    values = {}
    total = len(addresses)
    for index, addr in enumerate(addresses, 1):
        if not quiet:
            print(
                f"\rReading register {index}/{total} (0x{addr:04x}) ...",
                file=sys.stderr,
                end="",
                flush=True,
            )
        values[addr] = console.regget(addr)
    if not quiet:
        print("\r" + " " * 40 + "\r", file=sys.stderr, end="")
    return values


def decode_leds(values, ports):
    """Print the LED pad, LED set and port mapping held in the registers."""
    ledstr = []
    ledfmt = []
    print("LED pad Configuration:")
    for i in range(28):
        j = i % 5
        if j == 0:
            val = values[RTL837X_REG_LED_GLB_MUX_1 + (i // 5) * 4]
        ledval = (val >> (6 * j)) & 0x3F
        ledstr.append(f"{ledval:02x}")
        ledfmt.append(f"{i:02x}")
    print(f"{' '.join(ledfmt)}")
    print(f"{' '.join(ledstr)}")
    print(f".led_mux = {{ 0x{', 0x'.join(ledstr)} }},")

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
                values[RTL837X_REG_LED1_0_SET0 - 8 * i - ((id >> 1) << 2)]
                >> (16 * (id & 1))
            )
            valhi = (
                0xF
                & (
                    values[RTL837X_REG_LED3_0_SET1 - 4 * (i >> 1)]
                    >> (16 * (i & 1) + 4 * id)
                )
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
            if not tmpdefstr:
                tmpdefstr = "0"
            defvalstr.append(tmpdefstr)
        led_set_def.append("{ " + ",\n\t\t\t".join(defvalstr) + "\n\t\t      }")

        led_set.append(idval)
        led_set_str.append(idvalstr)
        idstr = "           ".join([f"{d:05x}" for d in idval])
        print(f"SET {i}:     {idstr}")

    portsel = values[RTL837X_LED_PORT_SET_SEL]
    for i in range(ports[0], ports[1] + 1):
        sel = (portsel >> (i << 1)) & 3
        print(f"Port {i}: SET {sel}: {', '.join(led_set_str[sel])}")

    print("\n\n\t.led_sets = { " + ",\n\t\t      ".join(led_set_def) + "\n\t\t},")


def print_pin_mux(values):
    """Print the pin mux registers and the function of every pin."""
    mux0 = values[RTL837X_PIN_MUX_0]
    mux1 = values[RTL837X_PIN_MUX_1]
    mux2 = values[RTL837X_PIN_MUX_2]
    glb_io_en = values[RTL837X_REG_LED_GLB_IO_EN]
    print("\nPin MUX Configuration:")
    print(f"PIN_MUX_0 (0x{RTL837X_PIN_MUX_0:04x}) = 0x{mux0:08x}   pads 0-29: 0 = LED function, 1 = GPIO")
    print(f"PIN_MUX_1 (0x{RTL837X_PIN_MUX_1:04x}) = 0x{mux1:08x}   one or two bits per pin, see below")
    print(f"PIN_MUX_2 (0x{RTL837X_PIN_MUX_2:04x}) = 0x{mux2:08x}   pins 30, 52-54: 0 = GPIO, 1 = ACL function")
    print(f"PIN_GLB_IO_EN (0x{RTL837X_REG_LED_GLB_IO_EN:04x}) = 0x{glb_io_en:08x}")
    print()
    print(f"{'Pin':<9}{'Field':<18}{'Value':<9}Function")
    for pin in range(64):
        field, val, function = pin_setting(pin, mux0, mux1, mux2)
        print(f"GPIO{pin:<5}{field:<18}{val:<9}{function}")


def print_gpio(values):
    """Print the GPIO registers, so pins can be surveyed in the same session."""
    base = dict(GPIO_REGS)
    print("\nGPIO Registers:")
    for name, addr in GPIO_REGS:
        print(f"{name}\t0x{addr:04x}\t0x{values[addr]:08x}")
    for name in ("GPIO_OUT", "GPIO_IN", "GPIO_OE"):
        bits = []
        for bank, offset in (("0", 0), ("1", 32)):
            reg = values[base[name + bank]]
            bits += [f"GPIO{n + offset}" for n in range(32) if reg & (1 << n)]
        print(f"{name} set: {', '.join(bits) if bits else '-'}")


def main():
    parser = argparse.ArgumentParser(
        description="Decode RTL837x LED wiring from a live serial console"
    )
    parser.add_argument(
        "-p",
        "--port",
        default="/dev/ttyUSB0",
        help="serial port of the switch console (default: /dev/ttyUSB0)",
    )
    parser.add_argument(
        "-b",
        "--baudrate",
        type=int,
        default=57600,
        help="console baudrate (default: 57600)",
    )
    parser.add_argument(
        "--cmd",
        action="append",
        metavar="TEMPLATE",
        help="register read command, {addr} is replaced by the hex address; "
        "repeatable, the forms are tried in order until one answers "
        "(default: 'regget 0x{addr:04x}', then 'regget {addr:04x}')",
    )
    parser.add_argument(
        "--char-delay",
        type=float,
        default=0.05,
        help="seconds between the characters of a command; the stock console "
        "drops characters if a command arrives as one burst, 0.05 measured "
        "clean while 0.03 lost one in eight (default: 0.05)",
    )
    parser.add_argument(
        "--ports",
        nargs=2,
        type=int,
        metavar=("MIN", "MAX"),
        default=None,
        help="port range to resolve to LED sets; by default it is taken from "
        "the chip model in register 0x0004: ports 0 8 on an RTL8373, 3 8 "
        "otherwise",
    )
    parser.add_argument(
        "--gpio",
        action="store_true",
        help="also dump the GPIO output/input/direction registers",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="seconds to wait for a command answer (default: 5)",
    )
    parser.add_argument(
        "--settle",
        type=float,
        default=0.4,
        help="quiet seconds that end an answer without a prompt (default: 0.4)",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=4,
        help="attempts per register, the console can drop a command (default: 4)",
    )
    parser.add_argument(
        "--lenient",
        action="store_true",
        help="also accept a bare hex value, for firmware that does not name the "
        "register in its answer; a mangled answer can be misread as a value",
    )
    parser.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="no progress output on stderr",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="print every console exchange on stderr",
    )
    args = parser.parse_args()

    cmds = args.cmd or ["regget 0x{addr:04x}", "regget {addr:04x}"]
    for template in cmds:
        if "{addr" not in template:
            parser.error(f"--cmd {template!r} has no {{addr}} placeholder")
    if not 0 <= args.char_delay <= 1:
        parser.error("--char-delay must be between 0 and 1 second")
    if args.settle > args.timeout:
        parser.error("--settle must not be larger than --timeout")
    if args.ports and (args.ports[0] > args.ports[1] or args.ports[1] > 63):
        parser.error("--ports takes a MIN MAX range between 0 and 63")

    try:
        console = RtlConsole(
            args.port,
            args.baudrate,
            cmds,
            args.char_delay,
            args.timeout,
            args.settle,
            args.retries,
            args.verbose,
            args.lenient,
        )
    except ConsoleError as exc:
        sys.exit(f"error: {exc}")

    addresses = register_addresses(args.gpio)
    try:
        if args.verbose:
            greeting = console.drain().strip()
            if greeting:
                print(f"--- console greeting\n{greeting}", file=sys.stderr)
        elif not args.quiet:
            estimate = len(addresses) * (args.char_delay * 12 + 0.15)
            print(
                f"Reading {len(addresses)} registers from {args.port} at "
                f"{args.baudrate} baud, about {estimate:.0f} s ...",
                file=sys.stderr,
            )
        else:
            console.drain()
        values = read_registers(console, addresses, args.quiet)
        model_info = values[RTL837X_REG_MODEL_NAME_INFO]
        ports = tuple(args.ports) if args.ports else model_ports(model_info)
        if not args.quiet:
            source = "--ports" if args.ports else f"model 0x{model_info:08x}"
            print(f"Decoding ports {ports[0]}..{ports[1]} (from {source})", file=sys.stderr)
        decode_leds(values, ports)
        print_pin_mux(values)
        if args.gpio:
            print_gpio(values)
    except ConsoleError as exc:
        sys.exit(f"error: {exc}")
    except KeyboardInterrupt:
        sys.exit(130)
    finally:
        console.close()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Probe the LED wiring of an RTL837x switch over the RTLPlayground console.

The script turns GPIO0..28 into plain GPIO outputs and sets them to one level
one after the other. Pins driven in earlier steps keep their level, so every
step changes exactly one pin and the one LED whose colour changes on the board
identifies the pad it is connected to. For every pin it asks which LED changed
and records the new answer code:

  * first line ``!  @  ... (``: colour 1 and colour 2 lit together, choose
    the matching port number (the shifted digits 1 to 9)
  * second line ``1  2  ... 9``: colour 1, choose the matching port number
  * third line ``a  b  ... i``: colour 2, choose the matching letter
  * fifth line ``r  s  ... z``: the LED went off, choose the matching letter
    for its port number
  * a single space (or ``.``): no LED changed

Before each walk starts, one line records the initial LED setting (all GPIOs at
the walk's base level) with the same codes. Every code names its port itself,
so whitespace between them is optional: ``1a`` and ``1 a`` both mean colour 1
and colour 2 at port 1, and ``r`` to ``z`` mark port 1 to 9 as off. A port with
no code at all counts as having no LED. This line is read with Return, the
per-GPIO answers afterwards are single key presses. When stdin is not a
terminal (piped input), a whole line is read per answer instead.

After adding all 29 pins at 1 one by one, the script drives all of them to 1
and repeats the walk with 0s, which covers LEDs that are wired active-low.
``--walk-0-first`` swaps the two phases, for boards whose LEDs are only lit
when their pad is low. The collected mapping is printed at the end (space
answers are ignored), then the saved GPIO/LED registers are put back.

The report ends with the decoded wiring: for every port the GPIOs that switch
its LEDs and, per colour, the levels at which those GPIOs turn it on and off.
Contradictory answers - a colour reported switched on (or off) at both levels,
or a step that claims a colour the port already showed - are listed as well,
because they usually mean a mistyped answer.

With ``--dry-run`` no serial port is opened: the ``regset`` commands are printed
instead and the LED questions are skipped, so the sequence can be checked first.

The console is the RTLPlayground firmware console. Its ``regset``/``regget``
commands take plain hex without a ``0x`` prefix (RTLPlayground's ``atoi_hex()``
rejects the ``x``). The console must be free - stop minicom/screen on the port
first - and, on boards that protect it with a login/password prompt, log in on
the console before starting the script. The script cannot log in for you.

Requirements: Python 3, pyserial (``apt install python3-serial``).
"""

import argparse
import os
import sys

from dec_leds_from_rtl837x_uart import (
    ConsoleError,
    RtlConsole,
    login_prompt,
    read_registers,
)

# The registers the probe changes, named as in rtl837x_regs.h. The LED pads
# 0..28 are GPIO0..28, and PIN_MUX_0 selects for every pad whether the LED
# engine or the GPIO block drives it. Pad 29, the RLDP LED on most boards, is
# left alone.
PIN_MUX_0 = 0x7F8C
GPIO_OUT0 = 0x003C
GPIO_OE0 = 0x004C
SAVED_REGS = [
    ("PIN_MUX_0", PIN_MUX_0),
    ("GPIO_OUT0", GPIO_OUT0),
    ("GPIO_OE0", GPIO_OE0),
]

PROBED_GPIOS = range(29)
GPIO_MASK = (1 << 29) - 1

# "GPIO0..28", used in the prompts and the report.
GPIOS_LABEL = f"GPIO0..{PROBED_GPIOS[-1]}"

# The shifted digits: both LEDs of a port lit at the same time. The position
# in the line is the port number, as in the colour lines below.
BOTH_COLOURS = "!@#$%^&*("

# The colour answer lines. Every character is one answer: the line gives the
# colour, the position in the line the port number.
COLOUR_LINES = [
    "".join(str(i) for i in range(1, 10)),
    "abcdefghi",
    "ABCDEFGHI",
]

# LED off, one character per port: the position in the line is the port number.
OFF_LINE = "rstuvwxyz"
CHOICES = set(BOTH_COLOURS + "".join(COLOUR_LINES) + OFF_LINE)

# The meaning of every answer line, printed in front of it.
LINE_LABELS = [
    "colour 1 + 2",
    "colour 1",
    "colour 2",
    "colour 3",
    "LED off",
]


def colour_table():
    """The answer lines, each with its meaning and two spaces between chars."""
    lines = [BOTH_COLOURS] + COLOUR_LINES + [OFF_LINE]
    labels = [f"({label})" for label in LINE_LABELS]
    width = max(len(label) for label in labels) + 2
    return "\n".join(
        label.ljust(width) + "  ".join(chars)
        for label, chars in zip(labels, lines)
    )


def parse_choice(answer):
    """The answer character, or None for "no change" (space or ".").

    Raises ValueError for anything that is not one of the accepted answers.
    """
    answer = answer.strip()
    if not answer or answer == ".":
        return None
    if len(answer) == 1 and answer in CHOICES:
        return answer
    raise ValueError(answer)


def port_of(choice):
    """The port number an answer code names."""
    if choice in BOTH_COLOURS:
        return BOTH_COLOURS.index(choice) + 1
    if choice in OFF_LINE:
        return OFF_LINE.index(choice) + 1
    for line in COLOUR_LINES:
        if choice in line:
            return line.index(choice) + 1
    return None


def state_of(choice):
    """The colours lit at the answered port after the step, () when off."""
    if choice in BOTH_COLOURS:
        return (1, 2)
    if choice in OFF_LINE:
        return ()
    for line, colour in zip(COLOUR_LINES, range(1, len(COLOUR_LINES) + 1)):
        if choice in line:
            return (colour,)
    return ()


def describe_choice(choice):
    """A readable description of a recorded answer."""
    if choice is None:
        return "no LED"
    if choice in OFF_LINE:
        return f"off, port {port_of(choice)}"
    if choice in BOTH_COLOURS:
        return f"colour 1 + 2, port {port_of(choice)}"
    for line, colour in zip(COLOUR_LINES, range(1, len(COLOUR_LINES) + 1)):
        if choice in line:
            return f"colour {colour}, port {port_of(choice)}"
    return choice


def parse_initial(answer):
    """The LED status per port from one typed line.

    The line is a sequence of answer codes and whitespace is optional: every
    code names its port itself, a colour code by its position in its colour
    line and an off code (r-z) by its letter. "1a" and "1 a" therefore mean
    the same, colour 1 and colour 2 at port 1. Returns a tuple per port, from
    port 1 up to the highest port named, of the colours lit there; an empty
    tuple means the port has no LED.

    Raises ValueError for anything that is not an accepted answer.
    """
    colours = {}
    off = set()
    for char in answer:
        if char.isspace():
            continue
        if char in OFF_LINE:
            port = OFF_LINE.index(char) + 1
            if port in colours:
                raise ValueError(f"port {port} is marked both lit and off")
            off.add(port)
            continue
        if char in BOTH_COLOURS:
            port, lit = BOTH_COLOURS.index(char) + 1, (1, 2)
        elif char in COLOUR_LINES[0]:
            port, lit = COLOUR_LINES[0].index(char) + 1, (1,)
        elif char in COLOUR_LINES[1]:
            port, lit = COLOUR_LINES[1].index(char) + 1, (2,)
        elif char in COLOUR_LINES[2]:
            port, lit = COLOUR_LINES[2].index(char) + 1, (3,)
        else:
            raise ValueError(
                f"{char!r} is not one of 1-9, a-i, A-I, a shifted digit or r-z"
            )
        if port in off:
            raise ValueError(f"port {port} is marked both lit and off")
        colours.setdefault(port, set()).update(lit)
    if not colours and not off:
        raise ValueError("enter LED codes, e.g. 1 for colour 1 or r for port 1 off")
    return [
        tuple(sorted(colours.get(port, ())))
        for port in range(1, max(set(colours) | off) + 1)
    ]


def describe_colours(colours):
    """The colours lit at one port, e.g. "colour 1 + 2" or "no LED"."""
    if not colours:
        return "no LED"
    return "colour " + " + ".join(str(c) for c in colours)


def describe_state(colours):
    """The colours lit at a port after a step, e.g. "colour 1 + 2" or "off"."""
    if not colours:
        return "off"
    return "colour " + " + ".join(str(c) for c in colours)


def read_register(console, addr):
    return read_registers(console, [addr], quiet=True)[addr]


def write_register(console, addr, value, retries, verbose=False):
    """Write one 32 bit register and confirm it with a read-back.

    RTLPlayground's regset takes plain hex without a ``0x`` prefix. The
    read-back is what makes a write trustworthy: this console can drop
    characters, and a shortened value would otherwise be accepted silently.
    """
    cmd = f"regset {addr:04x} {value:08x}"
    last_answer = ""
    for attempt in range(retries):
        answer = console.send(cmd)
        last_answer = answer
        if login_prompt(answer):
            raise ConsoleError(
                "the console is waiting for a login/password "
                f"({answer.strip()!r}): log in on the console first, "
                "then run this script again"
            )
        try:
            if read_register(console, addr) == value:
                return
        except ConsoleError as exc:
            last_answer = str(exc)
        if verbose:
            print(
                f"--- write {cmd} did not stick "
                f"(attempt {attempt + 1}/{retries}), retrying",
                file=sys.stderr,
            )
        console.drain()
    raise ConsoleError(
        f"could not write 0x{value:08x} to 0x{addr:04x}, "
        f"last answer: {last_answer!r}"
    )


def save_registers(console):
    """Read the GPIO/LED registers before anything is changed."""
    values = read_registers(console, [addr for _, addr in SAVED_REGS], quiet=True)
    return {name: values[addr] for name, addr in SAVED_REGS}


def configure_outputs(console, saved, retries, verbose=False):
    """Make GPIO0..28 GPIO outputs and latch all of them low.

    The output latch is set before the direction bits, so the pads do not
    briefly drive an old value when they become outputs; the pad mux is switched
    last, so the pins only leave the LED engine once they are ready to drive.
    """
    write_register(
        console, GPIO_OUT0, saved["GPIO_OUT0"] & ~GPIO_MASK, retries, verbose
    )
    write_register(
        console, GPIO_OE0, saved["GPIO_OE0"] | GPIO_MASK, retries, verbose
    )
    write_register(
        console, PIN_MUX_0, saved["PIN_MUX_0"] | GPIO_MASK, retries, verbose
    )


def restore_registers(console, saved, retries, verbose=False):
    """Put back everything that configure_outputs() and the walks changed."""
    write_register(console, GPIO_OUT0, saved["GPIO_OUT0"], retries, verbose)
    write_register(console, GPIO_OE0, saved["GPIO_OE0"], retries, verbose)
    write_register(console, PIN_MUX_0, saved["PIN_MUX_0"], retries, verbose)


def read_key(prompt):
    """Read one key press without waiting for Return.

    On a terminal the key is returned as soon as it is pressed and echoed, so
    answering does not need a following Return. When stdin is not a terminal
    (piped input, tests) a whole line is read instead, and many lines of input
    can drive the probe unattended.
    """
    if not sys.stdin.isatty():
        return input(prompt)

    print(prompt, end="", flush=True)
    if os.name == "nt":
        import msvcrt

        key = msvcrt.getwch()
        if key == "\x03":  # Ctrl-C is delivered as a character, not a signal
            raise KeyboardInterrupt
    else:
        import termios
        import tty

        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        try:
            tty.setcbreak(fd)
            key = os.read(fd, 1).decode("utf-8", "replace")
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)

    if not key:
        raise EOFError
    print(key)
    return key


def ask_initial(level):
    """Ask which LEDs are lit before a walk, with all GPIOs at the base level."""
    base = 0 if level else 1
    prompt = f"All {GPIOS_LABEL} at {base}: LED status (spaces optional): "
    print(colour_table())
    while True:
        try:
            answer = input(prompt)
        except EOFError:
            raise KeyboardInterrupt from None
        try:
            return parse_initial(answer)
        except ValueError as exc:
            print(f"invalid answer: {exc}", file=sys.stderr)


def ask_choice(gpio, level):
    """Ask which LED changed when this GPIO is driven to ``level``."""
    prompt = f"GPIO{gpio} -> {level}, which LED changed (space = no change): "
    while True:
        try:
            answer = read_key(prompt)
        except EOFError:
            raise KeyboardInterrupt from None
        if answer in ("\r", "\n"):
            # Return is not needed any more; ignore a stray one instead of
            # recording it as "no LED".
            continue
        try:
            return parse_choice(answer)
        except ValueError:
            print(
                "invalid answer: use one character from the answer lines, "
                "or a space when no LED changed",
                file=sys.stderr,
            )


def probe_walk(console, level, saved, retries, verbose=False, interactive=True):
    """Set GPIO0..28 to ``level`` one after the other and record the changes.

    ``level`` is 1 for the walking-1 phase and 0 for the walking-0 phase. All
    pins start at the opposite level; every step changes only the current pin,
    so the pins driven in earlier steps keep their level and each answer names
    the one LED that changed because of that single pin. A dry run sets
    ``interactive`` to False: no LED answers are collected.
    """
    other = 1 - level
    forced = 0 if level else GPIO_MASK
    base = saved["GPIO_OUT0"] & ~GPIO_MASK
    answers = {}
    print(
        f"\nWalking {level}s: one GPIO at {level} per step, "
        "pins of earlier steps keep their level."
    )
    for gpio in PROBED_GPIOS:
        if level:
            forced |= 1 << gpio
        else:
            forced &= ~(1 << gpio)
        value = base | forced
        if not interactive:
            print(f"GPIO{gpio} -> {level} (previous pins unchanged)")
        write_register(console, GPIO_OUT0, value, retries, verbose)
        if interactive:
            answers[gpio] = ask_choice(gpio, level)
        else:
            answers[gpio] = None
    return answers


class DryRunConsole:
    """Serial-console stand-in for --dry-run: print writes, open no port."""

    def __init__(self):
        self.regs = {addr: 0 for _, addr in SAVED_REGS}

    def drain(self):
        return ""

    def close(self):
        pass

    def send(self, cmd):
        parts = cmd.split()
        if len(parts) == 3 and parts[0] == "regset":
            addr = int(parts[1], 16)
            value = int(parts[2], 16)
            self.regs[addr] = value
            print(f"regset {parts[1]} {parts[2]}")
            return f"REGSET: {addr:04x}: VAL: {value:08x}\r\nRTL8373:"
        if len(parts) == 2 and parts[0] == "regget":
            addr = int(parts[1], 16)
            return f"REGGET: {addr:04x}: VAL: {self.regs.get(addr, 0):08x}\r\nRTL8373:"
        return "syntax error\r\nRTL8373:"

    def regget(self, addr):
        return self.regs.get(addr, 0)


def collect_transitions(answers, initial, level):
    """Every LED change of one walk, with the state before and after it.

    A port's LED keeps its colour between the steps that changed it, so the
    initial setting and the last answer for that port give the state before
    the current pin was changed. This is what makes a single answer readable:
    colours that appear in the step are switched on by the current pin, the
    ones that disappear are switched off by it, both at the step's level.
    """
    states = {port: colours for port, colours in enumerate(initial, 1)}
    transitions = []
    for gpio in PROBED_GPIOS:
        choice = answers[gpio]
        if choice is None:
            continue
        port = port_of(choice)
        before = states.get(port, ())
        after = state_of(choice)
        states[port] = after
        transitions.append((port, gpio, level, before, after, choice))
    return transitions


def decode_leds(walking_1, walking_0, initial_1, initial_0):
    """Per port, the GPIOs that switch its LEDs and how they switch them.

    Returns ``(ports, problems)``. ``ports`` maps every port the walk changed
    to ``{"gpios": {...}, "colours": {colour: {gpio: {"on": levels, "off":
    levels}}}}``. Only a step that visibly changed the LED says anything: an
    LED that stays in its colour while a pin is driven is not attributed to
    that pin, since the step does not show what the pin did.

    ``problems`` lists contradictions between the answers, which are most
    likely typing mistakes: a colour reported switched on (or off) at both
    levels, an answer that names a port but does not change its colour, or a
    GPIO reported for two different ports in the two walks.
    """
    transitions = collect_transitions(walking_1, initial_1, 1)
    transitions += collect_transitions(walking_0, initial_0, 0)
    ports = {}
    problems = []
    ports_of_gpio = {}
    for port, gpio, level, before, after, choice in transitions:
        info = ports.setdefault(port, {"gpios": set(), "colours": {}})
        info["gpios"].add(gpio)
        ports_of_gpio.setdefault(gpio, set()).add(port)
        switched_on = set(after) - set(before)
        switched_off = set(before) - set(after)
        if not switched_on and not switched_off:
            problems.append(
                f"GPIO{gpio} -> {level}: the answer {choice!r} says port {port} "
                f"shows {describe_state(after)}, which it already showed "
                "before the step"
            )
        for colour in switched_on | switched_off:
            switch = info["colours"].setdefault(colour, {}).setdefault(
                gpio, {"on": set(), "off": set()}
            )
            if colour in switched_on:
                switch["on"].add(level)
            if colour in switched_off:
                switch["off"].add(level)
    for gpio in sorted(ports_of_gpio):
        if len(ports_of_gpio[gpio]) > 1:
            named = " and ".join(f"port {p}" for p in sorted(ports_of_gpio[gpio]))
            problems.append(
                f"GPIO{gpio}: the answers name {named}, but a pad switches "
                "the LED of one port, so one of those answers is probably wrong"
            )
    for port in sorted(ports):
        for colour, by_gpio in sorted(ports[port]["colours"].items()):
            for gpio, switch in sorted(by_gpio.items()):
                for state in ("on", "off"):
                    if len(switch[state]) > 1:
                        levels = " and ".join(str(v) for v in sorted(switch[state]))
                        problems.append(
                            f"port {port}, colour {colour}: GPIO{gpio} switches "
                            f"it {state} at {levels}, so one of those answers "
                            "is wrong"
                        )
    return ports, problems


def describe_switch(switch):
    """The levels at which a GPIO turns a colour on and off, e.g.
    "on at 0, off at 1"."""
    parts = []
    if switch["on"]:
        parts.append("on at " + " and ".join(str(v) for v in sorted(switch["on"])))
    if switch["off"]:
        parts.append("off at " + " and ".join(str(v) for v in sorted(switch["off"])))
    return ", ".join(parts)


def print_decoded(ports, problems):
    """Print, for every port, the pins that control its colours."""
    print("\nDecoded LED control per port (from the changes above):")
    if not ports:
        print("    (no LED changed, so no probed GPIO controls one)")
    for port in sorted(ports):
        info = ports[port]
        gpios = ", ".join(f"GPIO{gpio}" for gpio in sorted(info["gpios"]))
        print(f"    port {port}: controlled by {gpios}")
        if not info["colours"]:
            print("        (the answers do not say which colour each pin switches)")
        for colour in sorted(info["colours"]):
            for gpio in sorted(info["colours"][colour]):
                switch = info["colours"][colour][gpio]
                print(f"        colour {colour}: GPIO{gpio} {describe_switch(switch)}")
    print("\nInconsistencies (possible input errors):")
    if not problems:
        print("    (none)")
    for problem in problems:
        print(f"    {problem}")


def print_report(walking_1, walking_0, initial_1, initial_0):
    """Print the mapping, leaving out the "no LED" answers."""
    print("\nCollected LED/GPIO mapping (space answers ignored):")
    for label, ports in (
        (f"initial, all {GPIOS_LABEL} at 0", initial_1),
        (f"initial, all {GPIOS_LABEL} at 1", initial_0),
    ):
        print(f"  {label}:")
        if not any(ports):
            print("    (no LEDs)")
        else:
            for port, colours in enumerate(ports, 1):
                print(f"    port {port}: {describe_colours(colours)}")
    for label, answers in (
        ("walking 1 (GPIOs added high one by one)", walking_1),
        ("walking 0 (GPIOs added low one by one)", walking_0),
    ):
        print(f"  {label}:")
        found = False
        for gpio in PROBED_GPIOS:
            choice = answers[gpio]
            if choice is None:
                continue
            found = True
            print(f"    GPIO{gpio}: {choice!r} ({describe_choice(choice)})")
        if not found:
            print("    (no LEDs)")
    print_decoded(*decode_leds(walking_1, walking_0, initial_1, initial_0))


def main():
    parser = argparse.ArgumentParser(
        description="Probe the LED wiring of an RTL837x switch over the "
        "RTLPlayground serial console"
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
        default=115200,
        help="console baudrate (default: 115200)",
    )
    parser.add_argument(
        "--char-delay",
        type=float,
        default=0.05,
        help="seconds between the characters of a command (default: 0.05)",
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
        help="attempts per register write (default: 4)",
    )
    parser.add_argument(
        "--walk-0-first",
        action="store_true",
        help="drive the pins to 0 one by one before walking a 1, for LEDs "
        "that only light when their pad is driven low",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print the regset commands without opening the serial port; the "
        "saved values are placeholders and the LED questions are skipped",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="print every console exchange on stderr",
    )
    args = parser.parse_args()

    if not 0 <= args.char_delay <= 1:
        parser.error("--char-delay must be between 0 and 1 second")
    if args.settle > args.timeout:
        parser.error("--settle must not be larger than --timeout")

    if args.dry_run:
        console = DryRunConsole()
        print(
            "Dry run: no serial port is opened; only the regset commands are "
            "printed and the LED questions are skipped."
        )
    else:
        try:
            console = RtlConsole(
                args.port,
                args.baudrate,
                ["regget {addr:04x}"],
                args.char_delay,
                args.timeout,
                args.settle,
                args.retries,
                args.verbose,
                False,
            )
        except ConsoleError as exc:
            sys.exit(f"error: {exc}")

    saved = None
    restore_failed = False
    try:
        console.drain()
        saved = save_registers(console)
        if args.dry_run:
            print("Assumed saved values (placeholders, no switch was read):")
        print(
            "Saved: "
            + " ".join(f"{name}=0x{value:08x}" for name, value in saved.items())
        )
        print(f"Configuring {GPIOS_LABEL} as GPIO outputs, all driven low ...")
        configure_outputs(console, saved, args.retries, args.verbose)

        interactive = not args.dry_run
        order = (0, 1) if args.walk_0_first else (1, 0)
        answers = {}
        initial = {}
        for position, level in enumerate(order):
            if level == 0:
                print(
                    f"\nDriving all {len(PROBED_GPIOS)} GPIOs high for the "
                    "walking-0 phase ..."
                )
                write_register(
                    console,
                    GPIO_OUT0,
                    (saved["GPIO_OUT0"] & ~GPIO_MASK) | GPIO_MASK,
                    args.retries,
                    args.verbose,
                )
            elif position:
                print(
                    f"\nDriving all {len(PROBED_GPIOS)} GPIOs low for the "
                    "walking-1 phase ..."
                )
                write_register(
                    console,
                    GPIO_OUT0,
                    saved["GPIO_OUT0"] & ~GPIO_MASK,
                    args.retries,
                    args.verbose,
                )
            if interactive:
                initial[level] = ask_initial(level)
            answers[level] = probe_walk(
                console, level, saved, args.retries, args.verbose, interactive
            )
        walking_1 = answers[1]
        walking_0 = answers[0]

        if interactive:
            print_report(walking_1, walking_0, initial[1], initial[0])
    except ConsoleError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\ninterrupted", file=sys.stderr)
        sys.exit(130)
    finally:
        if saved is not None:
            try:
                print("Restoring saved GPIO/LED settings ...")
                restore_registers(console, saved, args.retries, args.verbose)
            except ConsoleError as exc:
                restore_failed = True
                print(f"error: could not restore settings: {exc}", file=sys.stderr)
        console.close()

    if restore_failed:
        sys.exit(1)
    if args.dry_run:
        print("\nDry run complete; no switch was touched.")


if __name__ == "__main__":
    main()

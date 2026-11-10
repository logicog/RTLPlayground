# Probing the LED wiring with `probe_leds_from_rtl837x_uart.py`

`probe_leds_from_rtl837x_uart.py` maps the LEDs of a board to the GPIO pads
that drive them, without an I2C dongle and without knowing the stock
configuration. It drives the pads one at a time and asks which LED changed, then
decodes the answers into "port N is controlled by GPIO a, colour 1 is on when
GPIO a is low" style statements.

Use it for a board whose `.led_sets`/`.led_mux` are unknown, or to confirm a
machine definition against the real hardware. It talks to the **RTLPlayground**
console, since it needs `regset` to drive the pins; it is not for the stock
firmware.

## Requirements

  - Python 3 and `pyserial` (`apt install python3-serial` or
    `pip3 install pyserial`)
  - RTLPlayground firmware running on the switch
  - A free console: stop `minicom`/`screen` on that port first. On boards that
    password-protect the console, log in there first - the script cannot log in
    for you and stops when it sees a login prompt.
  - The board in front of you: every pin has to be answered by eye

No LED is required on pads you do not care about; answer with a space.

## What the script changes

Three registers, saved first and put back at the end - also when the run is
interrupted with Ctrl-C:

| Register | Address | Change |
|----------|---------|--------|
| `GPIO_OUT0` | `0x003c` | output latch of GPIO0..28 |
| `GPIO_OE0` | `0x004c` | GPIO0..28 become outputs |
| `PIN_MUX_0` | `0x7f8c` | GPIO0..28 leave the LED engine and become plain GPIOs |

The output latch is written before the direction bits and the pad mux last, so
the pads never briefly drive an old value or leave the LED engine before they
are ready. Every write is read back; if the console drops a character the write
is retried (`--retries`, default 4), and the run aborts instead of continuing
with a value that did not stick.

Pads 27 and 28 are probed like the port pads, so on a board that wires them to
`LED_27` and the system LED (`LED_28_SYS`) those lamps are driven by the walk
too and appear in the report. Pad 29 (`LED_29`, the RLDP LED on most boards) is
left alone: the mask covers bits 0..28 of `PIN_MUX_0`, so the mux bit of that
pad stays as it was.

## How a walk works

1. All probed pins are driven to the opposite level: 0 before the walking-1
   phase, 1 before the walking-0 phase.
2. The script asks for the LED status of the board in that state.
3. It then changes **one** pin per step - GPIO0, GPIO1, … GPIO28 - and leaves
   the pins of earlier steps at the walk's level. The output value of the first
   steps is `0x1`, `0x3`, `0x7`, `0xf`, … (walking 1) or `0x7fffffe`,
   `0x7fffffc`, `0x7fffff8`, … (walking 0).
4. For every step it asks which LED changed.

Because only one pin changes per step, at most one LED can change, and the
answer names the port and the colour that pin controls. After the walking-1
phase the script drives all pins high and repeats the walk with 0s, which covers
LEDs wired active-low. `--walk-0-first` swaps the phases, which is useful on
boards whose LEDs are only lit while their pad is low, so the interesting phase
runs first.

## Answer codes

The codes are printed once per phase, in front of the initial-state question:

```
(colour 1 + 2)  !  @  #  $  %  ^  &  *  (
(colour 1)      1  2  3  4  5  6  7  8  9
(colour 2)      a  b  c  d  e  f  g  h  i
(LED off)       r  s  t  u  v  w  x  y  z
```

Every code names its own port, so there is nothing to count:

| Code | Meaning |
|------|---------|
| `1` … `9` | colour 1 at port 1 … 9 |
| `a` … `i` | colour 2 at port 1 … 9 |
| `!` … `(` | colour 1 and colour 2 lit together at port 1 … 9 (the shifted digits) |
| `r` … `z` | port 1 … 9 has no LED / its LED went off |
| space or `.` | no LED changed |

The "colour" numbers are only labels for the answer lines: the script
does not know which lamp is which, you pick one line per lamp and keep using it
for the whole run. A board with a single LED per port uses one line, a port with
two lamps (e.g. amber and green) uses two. If two lamps of one port are lit at
the same time, `!` … `(` records both at once.

### Initial LED status line

Before each walk the script asks for the LEDs lit while all probed pins are at
that walk's base level. The answer uses the same codes, and whitespace between
them is optional - `1a` and `1 a` are the same answer:

  - `1` colour 1, `a` colour 2 at the code's port
  - `1a` or `!` - colour 1 and colour 2 lit at that port
  - `r` … `z` - no LED at that port
  - a port not mentioned at all counts as having no LED, so `1a` alone also
    means the other eight ports are dark

The line is finished with Return. `1 s a` and `1sa` both mean colour 1 at port 1,
no LED at port 2, colour 2 at port 3. Marking a port both lit and off
(`1r`, `r1`) is rejected, as is any character outside the codes.

### Per-pin answers

While walking, the answer is the new code of the one LED whose colour changed
because of the current pin - or `r` … `z` when that LED went off. A single key
press is the whole answer; no Return is needed and the key is echoed. A stray
Return is ignored, and an invalid key is reported and asked again.

When stdin is not a terminal (piped input, tests, scripts), one whole line is
read per answer instead, so a run can be driven from a file.

## Output

The report is printed after the last pin:

```
Collected LED/GPIO mapping (space answers ignored):
  initial, all GPIO0..28 at 0:
    port 1: no LED
    ...
  walking 1 (GPIOs added high one by one):
    GPIO3: 'a' (colour 2, port 1)
    GPIO4: 'r' (off, port 1)
  walking 0 (GPIOs added low one by one):
    ...

Decoded LED control per port (from the changes above):
    port 1: controlled by GPIO3, GPIO4
        colour 1: GPIO3 on at 0, off at 1
        colour 2: GPIO4 on at 0, off at 1

Inconsistencies (possible input errors):
    (none)
```

### How the decode works

A port's LED keeps its colour until a step changes it, so the initial setting
plus the last answer for that port give the state *before* the current pin was
driven. Colours that appear in that step were switched on by the pin at the
step's level, colours that disappear were switched off by it. Using both walks,
`on at 0, off at 1` means active low (drive the pad low to light the LED) and
`on at 1, off at 0` means active high (a transistor inverts the pad).

Only visible changes are attributed: an LED that keeps its colour while a pin is
driven says nothing about that pin, so a colour that needs two pins lit at once
never appears in the report. Pins that never changed an LED are not listed.

### Inconsistencies

Answers that contradict each other are listed separately instead of being
silently accepted. They usually mean a mistyped code:

  - a colour reported switched on (or off) at both 0 and 1 by the same GPIO;
  - an answer naming a port whose LED already showed exactly that colour before
    the step, so the step changed nothing;
  - the same GPIO reported for two different ports across the two walks, since
    one pad drives the LED of one port.

## Usage

```bash
# Default console port (/dev/ttyUSB0) and baudrate (115200)
python3 probe_leds_from_rtl837x_uart.py

# Another port, with a log of every console exchange
python3 probe_leds_from_rtl837x_uart.py -p /dev/ttyUSB1 -v

# Print the register writes without opening the serial port
python3 probe_leds_from_rtl837x_uart.py --dry-run

# Add the pins as 0s first (LEDs wired active-low)
python3 probe_leds_from_rtl837x_uart.py --walk-0-first
```

`--dry-run` prints one `regset <addr> <value>` line per write, including the
accumulating walking phases and the restore. The saved values are placeholders
and the LED questions are skipped, so the sequence can be checked before
touching a switch.

## Options

| Option | Meaning |
|--------|---------|
| `-p` / `--port` | serial port (default: `/dev/ttyUSB0`) |
| `-b` / `--baudrate` | console baudrate (default: 115200) |
| `--char-delay` | seconds between the characters of a command (default: 0.05); 0 sends a command in one burst |
| `--timeout` | seconds to wait for a command answer (default: 5) |
| `--settle` | quiet seconds that end an answer without a prompt (default: 0.4) |
| `--retries` | attempts per register write (default: 4) |
| `--walk-0-first` | run the walking-0 phase before the walking-1 phase |
| `--dry-run` | print the `regset` commands, open no serial port, skip the LED questions |
| `-v` / `--verbose` | print every console exchange on stderr |

A run takes a while: 29 steps per phase, each a write plus a read-back, plus
however long you need to look at the board.

## Example session

```
Saved: PIN_MUX_0=0x00000000 GPIO_OUT0=0x00100000 GPIO_OE0=0x000fc000
Configuring GPIO0..28 as GPIO outputs, all driven low ...
(colour 1 + 2)  !  @  #  $  %  ^  &  *  (
...
All GPIO0..28 at 0: LED status (spaces optional): 1r
Walking 1s: one GPIO at 1 per step, pins of earlier steps keep their level.
GPIO0 -> 1, which LED changed (space = no change): r
GPIO1 -> 1, which LED changed (space = no change): r
...
Walking 0s: one GPIO at 0 per step, pins of earlier steps keep their level.
GPIO0 -> 0, which LED changed (space = no change): 1
...
Restoring saved GPIO/LED settings ...
```

## Limits and troubleshooting

  - **Nothing lights up.** Try `--walk-0-first`: some boards only light an LED
    while the pad is low, and the first phase (all pins at 0 for a walking-1
    walk) may itself be the state in which the LEDs are off.
  - **Every answer is a space.** The pads may not be wired to an LED directly
    (LED engine outputs, transistors, or the LED needs another pin low), or the
    board's LEDs are on pad 29 (`LED_29`, the RLDP LED), which is not probed.
  - **The console prints a login prompt.** Log in on the console with your
    terminal program before running the script; the console has to stay
    unlocked for the whole run.
  - **Writes keep failing.** Lower `--char-delay` (or raise it), raise
    `--timeout`, or raise `--retries`. The script aborts rather than continue
    when a register does not read back as written, and always tries to restore
    the saved values.
  - **The decoded lines look wrong.** Check the
    `Inconsistencies` block first: a single mistyped code there usually explains
    the odd GPIO, and rerunning the phase is cheaper than debugging the
    definition.

The registers the script writes are runtime state only - nothing is stored in
flash, and a reboot restores the board's own configuration even if the final
restore fails.

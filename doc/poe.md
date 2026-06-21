# PoE — generic framework

How Power-over-Ethernet is structured and managed in RTLPlayground. This is the chip-agnostic
base: a board declares its PSE in the machine descriptor, a per-chip **driver** implements a
small interface, and the `poe` console command, the `/poe.json` endpoint and the web page are
all chip-agnostic. Anything specific to a particular PSE chip lives in its own driver document:

- **RTL8238B** — [poe-rtl8238b.md](poe-rtl8238b.md).

## Opt-in per machine

PoE is **opt-in per machine**: a board declares its PSE in the machine descriptor, and all PoE
code (the driver, the `poe` console command, `/poe.json`, the web page, the firmware-image
embedding) is compiled in only for those boards. Machines without PoE — or with hardware-only
PoE that isn't software-controlled — compile none of it.

## The machine descriptor

PoE is described per board in the machine descriptor ([machine.h](../machine.h),
[machine.c](../machine.c)):

```c
struct poe_config {
    uint8_t chip;        // POE_NONE / POE_RTL8238B / POE_<chip>
    uint8_t addr0;       // first controller address (bus-specific, e.g. I2C 0x20)
    uint8_t addr1;       // second controller address, 0 = single
    uint8_t n_ports;     // total PoE ports
};
```

A board that wants software-controlled PoE:

1. **Selects a driver** — adds its `MACHINE_*` to the `POE_CHIP_<chip>` map in
   [machine.h](../machine.h). The chip macro chooses which driver source compiles (e.g.
   `poe_rtl8238b.c`) and implies **`POE_PRESENT`**, the generic gate that `#ifdef`-includes the
   chip-agnostic consumers: the `poe` console command, the `/poe.json` endpoint, the boot
   bring-up, and the firmware-image embedding for chips that need one. Boards without a
   `POE_CHIP_*` macro compile no PoE code at all.
2. **Describes the hardware** — sets the `.poe` field in its `struct machine` initializer
   (chip, addresses, port count).

The boot bring-up is then **data-driven**: `if (machine.poe.chip != POE_NONE) poe_bringup();`.

## The chip-agnostic interface

A driver implements the small interface in [poe.h](../poe.h); the console, web page and boot
call **only** this and never touch the controller themselves:

```c
void poe_bringup(void);                              // bring PoE up (boot + `poe load`)
poe_port_num, poe_port_on;   void poe_port_set(void);   // enable/disable one port
poe_global_on;               void poe_global_set(void); // enable/disable all ports
// normalized per-port status — typed, chip-agnostic values:
poe_st_admin, poe_st_on, poe_st_class, poe_st_volt, poe_st_ma;   void poe_get_port(void);
```

**How** a driver talks to its chip — direct register I/O, a 12-byte host-command MCU protocol,
etc. — is entirely the driver's business and never leaks past this interface. The driver decodes
its hardware into the normalized `poe_st_*` values (admin / delivering / class / volts / mA), so
`/poe.json` and the web UI present PoE status with **no** knowledge of the controller.

## Adding hardware

**Another board with an already-supported chip:** add its `MACHINE_*` to that chip's
`POE_CHIP_<chip>` map in machine.h, set `.poe` (addresses / port count), and — if the chip needs
an embedded firmware image — add it to that chip's machine list in the Makefile and supply the
image (see the driver doc). No driver code changes. *(A board that is identical to an existing non-PoE machine plus a PSE can reuse
that machine's descriptor — see how the KP-9000-9XHPML-X reuses the KP-9000-9XHML-X block in
[machine.h](../machine.h)/[machine.c](../machine.c).)*

**A different PSE chip:** add a `POE_<chip>` value to the `chip` enum, write a `poe_<chip>.c`
implementing [poe.h](../poe.h), gate it with its own `POE_CHIP_<chip>` macro, and document it in
`doc/poe-<chip>.md`. One chip driver is compiled per build (compile-time selection, no runtime
dispatch — 8051 code-budget).

## Firmware image (if the chip needs one)

Some PSEs run a volatile MCU firmware image that must be uploaded at every boot. Such an image is
proprietary and is **not** committed; the user supplies it and the Makefile embeds it at a fixed
flash offset — only for the chip's listed machines, and only when the image file is present. The
offset and the machine list are **chip-specific** (named per chip in the Makefile), and the offset
must match the address the driver reads from. See the driver doc for the concrete knobs (RTL8238B →
[poe-rtl8238b.md](poe-rtl8238b.md)).

How to obtain/extract the image is chip-specific — see the driver document.

## Using it

PoE comes up automatically at boot on a PoE machine.

**Console** — deliberately chip-agnostic; only the operations any driver provides:

| Command | Purpose |
|---------|---------|
| `poe load` | Re-run the full bring-up (e.g. after a cold power-cycle). |
| `poe port <n> <on\|off>` | Enable/disable one port. |
| `poe global <on\|off>` | Enable/disable all ports. |

**Web** — the **PoE** page shows total consumption and a per-port status table (state / power /
class / W / V / mA) with per-row and global enable/disable, and degrades to a "no PoE controller
on this device" notice on non-PoE machines. `GET /poe.json` returns the **driver-normalized**
per-port status (one object per port: `port, admin, on, class, v, ma`) and takes no query
parameters — the controller is never exposed over HTTP; the endpoint only serves the typed values
the driver already produced. Power (W) and the total are just `v·ma`, derived by the consumer.

**Persisting settings.** At boot PoE comes up with **all ports enabled** (the bring-up default);
runtime changes made via the console or web are *not* auto-saved — exactly like the IP / gateway /
etc., whose persistent source is `config.txt`. That file is replayed at boot **after** the PoE
bring-up, so adding `poe` commands to it makes a state stick across reboots. For example, to keep
every port on except port 3, put in `config.txt`:

    poe port 3 off

or to run only ports 1–2:

    poe global off
    poe port 1 on
    poe port 2 on

(The driver's bring-up enables everything; the replayed `config.txt` commands then refine it — so
no per-port state lives in the driver, only the chip-agnostic `poe` operations it exposes.)

## Building & flashing

Build and flash exactly as for any other machine — see the [README](../README.md) (*"Compiling…"*
and *"Installation through the Web interface"*). The only PoE-specific points:

- **Select your PoE machine** (e.g. `KP_9000_9XHPML_X_V3_1`) in [machine.h](../machine.h), or pass
  it on the command line (`make MACHINE=KP_9000_9XHPML_X_V3_1`).
- **If the chip needs a firmware image**, obtain it first (see the driver doc); the Makefile then
  embeds it automatically for that chip's listed machines.

After flashing, **cold power-cycle** the device (the PSE's loaded state does not survive — and is
only re-uploaded — across a physical power-cut).

## Troubleshooting

- **A web page shows "no PoE controller":** that build's machine has no `.poe` descriptor (PoE
  not enabled for it), or `/poe.json` is compiled out.
- Chip-specific symptoms (image rejected, ports not powering, register dumps) live in the driver
  document.

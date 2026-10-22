# Flow control

The copper ports advertise symmetric 802.3x pause during autonegotiation. The
link uses pause only where the link partner offers it as well.

```
port <n> fc on|off       # advertise pause on a copper port, or not
port <n> show            # ends with "Pause negotiated: TX RX", "TX", "RX" or "none"
```

Switching a port renegotiates its link, which drops it for a moment. The
choice is kept when the speed or duplex of the port changes later.

## Web interface

The Ports page shows flow control and EEE next to the other port settings, each
as a switch followed by its state:

| State | Meaning |
|---|---|
| TX, RX, TX/RX (flow control) or Active (EEE) | in use on the link; TX means the port sends pause frames, RX that it honours them |
| Inactive | switched on, but not in use on this link (no link, or the partner does not take part) |
| Off | switched off, or the port is disabled |

The ⓘ button next to each state opens what this port and its link partner
advertise: the pause flags (Pause, Asym) for flow control, the speeds for EEE.
The ⓘ next to the link shows the speeds the port advertises, with the one it
linked at marked in the colour of its tile in the port strip.

SFP ports have no autonegotiated pause and no EEE, so they cannot be switched;
the page still shows the pause directions their MAC reports as negotiated.

All settings of the page are a draft until Apply. Apply sends, for each port
that was changed, the whole state the page shows, so the port ends up as
displayed. This renegotiates the link of every changed port, even when only its
name or MTU changed. The commands are listed above the Apply button before they
are sent. Leaving the page with changes that were not applied asks first.

Settings made on the page or on the console are kept only once the
configuration is saved.

## Data

`/status.json` carries both features as one number per port:

| Field | Bits |
|---|---|
| `fc` | 0 pause and 1 asymmetric pause advertised, 2 and 3 the same offered by the link partner, 4 TX and 5 RX negotiated (`PAUSE_*` in `rtl837x_pause.h`) |
| `eee` (copper only) | 0-2 EEE advertised at 100M, 1G and 2.5G, 3-5 the same advertised by the link partner, 6 EEE in use (`EEE_*` in `httpd/page_impl.h`) |

The registers involved:

| Register | What it holds |
|---|---|
| MMD 7.0x0010, bits 10/11 | pause and asymmetric pause advertised by the PHY |
| MMD 7.0x0013, bits 10/11 | pause and asymmetric pause offered by the link partner |
| `0x63FC` / `0x6400` | TX and RX pause negotiated, bit N is chip port N |
| MMD 7.0x003c-0x003f | EEE advertised by the PHY and by the link partner |
| `0x642C` | EEE in use, bit N is chip port N |

Flow control as the reaction to an ingress bandwidth limit is a separate
setting, see [Bandwidth](bandwidth.md).

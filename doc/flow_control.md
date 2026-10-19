# Flow control

The copper ports advertise symmetric 802.3x pause during autonegotiation. The
link uses pause only where the link partner offers it as well.

```
port <n> fc on|off       # advertise pause on a copper port, or not
port <n> show            # ends with "Pause negotiated: TX RX", "TX", "RX" or "none"
```

Switching a port renegotiates its link, which drops it for a moment. The
choice is kept when the speed or duplex of the port changes later.

The Flow control page of the web interface shows the same per port:

- Advertised: the pause flags this switch offers (Pause, Asym).
- Link partner: the pause flags the other end offers.
- Negotiated: the pause directions in use on the link, shown as TX, RX, TX/RX
  or off where `port <n> show` prints `TX`, `RX`, `TX RX` or `none`. TX means
  this port sends pause frames, RX means it honours them.

SFP ports have no autonegotiated pause, so they cannot be switched; the page
still shows what their MAC reports as negotiated.

Settings made on the page or on the console are kept only once the
configuration is saved.

The registers involved:

| Register | What it holds |
|---|---|
| MMD 7.0x0010, bits 10/11 | pause and asymmetric pause advertised by the PHY |
| MMD 7.0x0013, bits 10/11 | pause and asymmetric pause offered by the link partner |
| `0x63FC` / `0x6400` | TX and RX pause negotiated, bit N is chip port N |

Flow control as the reaction to an ingress bandwidth limit is a separate
setting, see [Bandwidth](bandwidth.md).

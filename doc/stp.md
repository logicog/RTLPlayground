# Spanning Tree (STP / RSTP)

The switch can take part in a spanning tree (IEEE 802.1D / 802.1w) so that
redundant links between bridges are blocked instead of forming a loop. The
implementation elects a root bridge from the BPDUs it receives, promotes ports
to forwarding once their listen period expires, ages the root out when it goes
silent, and blocks a port on which it sees its own BPDU.

It is deliberately simple: there is no proposal/agreement handshake and no full
port-role machine. What it does do reliably is stop a cabling loop from melting
the network, and interoperate with neighbouring bridges as a well-behaved
(if unexciting) participant.

> **Before you enable it on a switch you reach over the network**: read the
> [management failsafe](#management-failsafe) section. The management VLAN
> rides a port that STP can block.

## Quick start

```
stp on                  # start participating
stp off                 # stop, all ports back to forwarding
```

Live status is on the Spanning Tree page of the web UI (or `/stp.json`).

With no other bridge around, the switch elects itself root and every port ends
up forwarding — you can leave it on safely. Put the settings in the startup
config to make them survive a reboot:

```
stp prio 15
stp port 1 edge on
stp on
```

## Hardware background

BPDUs are addressed to `01:80:C2:00:00:00`, a reserved link-local group. The
ASIC's Reserved-Multicast action for that address decides what happens to the
frame.

Forwarding to the CPU port works normally — the 8051 sits behind an ordinary
port of the internal switch and is an ordinary member of a forwarding mask.
What does not work is the *trap* action, which is a separate mechanism: its
destination is an external CPU attached to a physical port
(`cpuTag_externalCpuPort_set`, `EXT_CPU_CTRL` in the vendor SDK), which these
boards do not populate. Measured on a SWTGW218AS: with the RMA action set to
trap, zero frames arrive at the 8051, including with `CPU_PMSK` widened and the
external-CPU destination pointed at both `0xF` and `9`; with the *forward*
action plus the L2 entry below, they arrive. The ACL trap behaves the same way,
measured on the neighbouring reserved group `01:80:C2:00:00:02`: a rule matching
it intercepts the frames — the LACP receive counters stop advancing while the
rule is enabled and resume the moment it is disabled — but they never reach the
8051, with `FWD_INT_TRAP` and with `REDIRECT` aimed at the CPU port alike.

Delivery therefore uses the *forward* action, constrained to the CPU port
by a static L2 multicast entry (`port_l2mc_set()`), one per VLAN in use:

* while STP runs, the entry's member mask is the CPU port only — BPDUs reach
  the CPU and are not flooded to other ports, as a participating bridge
  requires;
* with STP off, the same entries are retargeted to all ports, restoring the
  transparency an unmanaged switch is expected to have, so a surrounding
  spanning tree can span *through* this device.

Because delivery rides the forward action, a BPDU is an ordinary frame to the
ingress pipeline and is subject to the port's acceptable-frame-type setting.
BPDUs are untagged by definition, so a port configured to admit tagged frames
only (`ingress <port>t`) will never deliver one: a port left on the default
auto edge turns edge after three seconds of silence, one with edge switched off
sits out the full forward delay instead, and either way the bridge elects
itself root no matter what the neighbour sends. `stp_setup()` prints a warning for every
STP-enabled port in that state. On a normal bridge this cannot happen, since
BPDUs are consumed before any VLAN classification; here it is a direct
consequence of the delivery path above.

Port states live in `RTL837X_MSTP_STATES (0x5310)`, two bits per port:
`00` disabled, `01` blocking, `10` learning, `11` forwarding. Note that a port
held in blocking also drops frames the CPU injects into it, so a blocked port
cannot transmit BPDUs of its own.

## Timers

`stp_timers()` runs at 50 Hz (the main loop idles on the 200 Hz system tick and
STP is called every fourth pass), which is what `STP_HZ` in `rtl837x_stp.h`
encodes. All configured values are in seconds:

| setting | default | range |
|---|---|---|
| `stp hello <n>` | 2 | 1–10 |
| `stp maxage <n>` | 20 | 6–40 |
| `stp fwd <n>` | 15 | 4–30 |
| `stp txhold <n>` | 6 | 1–10 |

A port entering the tree spends `fwd` seconds in blocking before it forwards
(an edge port skips the wait). Root information is discarded after `maxage`
seconds without a BPDU, and the switch then reclaims the root role.

## Bridge settings

```
stp prio <0-15>         # bridge priority = n * 4096, default 8 (32768)
stp version rstp|stp    # RST BPDUs (default) or legacy Config BPDUs
stp hello|maxage|fwd|txhold <seconds>
```

The bridge with the lowest priority wins the root election; ties are broken by
the MAC address. If you do not want this switch to become the root of an
existing network, give it a worse priority than the current root — `stp prio 15`
(61440) is the usual "never me" value.

## Per-port settings

```
stp port <1-9> on|off              # take part in STP, or stay plain forwarding
stp port <1-9> edge on|off|auto    # host-facing port handling (default: auto)
stp port <1-9> cost <0-200000000>  # path cost, 0 = automatic (20000)
stp port <1-9> prio <0-240>        # port priority, steps of 16
stp port <1-9> guard none|bpdu|root
stp port <1-9> filter on|off       # neither send nor accept BPDUs
stp port <1-9> p2p auto|on|off
```

**edge** — an edge port goes forwarding immediately and does not trigger a
topology change when it comes and goes; `auto` promotes a port to edge after
three seconds without a BPDU, and demotes it as soon as one arrives. Use
`edge on` for ports where only hosts are attached.

**guard** — `bpdu` disables a port as soon as a BPDU arrives on it (a host port
should never see one); `root` keeps a port from ever becoming the path to the
root, which protects an existing topology from a newly attached bridge that
claims a better priority.

**filter** — the port neither sends nor accepts BPDUs. Useful when the device
on the far side reacts badly to them (some unmanaged switches with loop
prevention cut the link) but you still want STP on the rest of the ports.

## Management failsafe

Enabling STP on a switch you administer over the network is a genuine risk: the
management VLAN rides a port that STP may put into blocking, and once that
happens the way back is a power cycle.

The firmware therefore runs a commit-confirm watchdog. Enabling STP, by hand or
from the startup config, arms a one-shot window of `stp failsafe <seconds>`
(default 180). One HTTP request inside the window confirms that management
survived the new tree and disarms the watchdog until the next enable; a window
with no management activity disables STP and restores forwarding. After the
confirmation STP runs unsupervised, so a quiet network no longer loses its
tree to three minutes of nobody looking at the web UI.

```
stp failsafe 180        # length of the armed window after enabling (0 = never armed)
```

Any later event that newly takes a port out of forwarding arms the window
again: a port rejoining via `stp port <n> on`, root guard firing, the loop
latch. If management traffic keeps flowing past the new block, the very next
request confirms and disarms; if the block cut it, the silent window restores
forwarding as above. A stable network with nothing newly blocked never re-arms.

A command executed on the serial console also confirms, on the grounds that an
operator with out-of-band access does not need the automatic restore; the
command that enabled STP does not count, only activity after it.

A headless switch that nobody confirms over HTTP should set `stp failsafe 0`,
otherwise a reboot with STP in the startup config disables it again three
minutes later. Setting a new value while STP runs arms a fresh window.

The status page shows whether the failsafe has tripped since STP was last
enabled.

## Status

The Spanning Tree page shows the elected root (priority and MAC), the path cost
to it, the root port, the topology-change counter and, per port, the live state
read from the ASIC together with the configured options. The same data is
available as JSON:

```
GET /stp.json
```

## Limitations

* One spanning-tree instance; no MSTP, no per-VLAN trees.
* No proposal/agreement handshake — an RST-capable neighbour will still
  converge, but through the timers rather than the fast transition.
* Port roles are approximated: the root port and designated ports are
  distinguished, alternate/backup are not.
* A port in blocking cannot transmit, so a blocked port stops announcing
  itself; recovery relies on the listen timer rather than on a neighbour's
  agreement.

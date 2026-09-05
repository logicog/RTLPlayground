# Spanning Tree (STP / RSTP)

The switch can take part in a spanning tree (IEEE 802.1D / 802.1w) so that
redundant links between bridges are blocked instead of forming a loop. The
implementation elects a root bridge from the BPDUs it receives, promotes ports
to forwarding once their listen period expires, ages the root out when it goes
silent, and blocks a port on which it sees its own BPDU.

STP can be enabled and controlled via the web interface or the command line,
as follows:

## Quick start

```
stp on                  # start participating
stp off                 # stop, all ports back to forwarding
```

Live status is on the Spanning Tree page of the web UI (or `/stp.json`),
and on the serial console via `stp status`.

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

Forwarding to the CPU port works normally: the 8051 sits behind an ordinary
port of the internal switch and is an ordinary member of a forwarding mask.
The *trap* action does not deliver to it. Its destination is an external CPU
attached to a physical port (`cpuTag_externalCpuPort_set`, `EXT_CPU_CTRL` in
the vendor SDK), which these boards do not populate. The ACL trap and
redirect actions do not deliver to the 8051 either.

Delivery therefore uses the *forward* action, constrained to the CPU port
by a static L2 multicast entry (`port_l2mc_set()`), one per VLAN in use:

* while STP runs, the entry's member mask is the CPU port only — BPDUs reach
  the CPU and are not flooded to other ports, as a participating bridge
  requires;
* with STP off, the same entries are retargeted to all ports, restoring the
  transparency an unmanaged switch is expected to have, so a surrounding
  spanning tree can span *through* this device.

A BPDU delivered this way is an ordinary frame to the port's ingress logic
and passes through its acceptable-frame-type filter. BPDUs are untagged, so a
port set to admit tagged frames only (`ingress <port>t`) never delivers one
to the CPU. `stp_setup()` prints a warning for every STP-enabled port in that
state.

Port states live in `RTL837X_MSTP_STATES (0x5310)`, two bits per port:
`00` disabled, `01` blocking, `10` learning, `11` forwarding. A port in
blocking forwards nothing between ports, but it still sends what the CPU
hands it and still passes a received BPDU up to the CPU, which is what lets
loop detection go on working on a port it has already blocked.

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

## Topology changes

A change on a local non-edge port (the link coming or going, a port promoted
to forwarding) flushes the addresses learned on it and sets the TC flag in
our BPDUs for `maxage + fwd` seconds. A TC flag received in a BPDU is passed
on: the switch flushes the other non-edge ports once and keeps the flag in
its own BPDUs until one hello after the last flagged frame, so the
notification crosses the switch instead of dying at it. A legacy TCN is
acknowledged with TCA and then treated like a local change.

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

**edge** — an edge port forwards immediately and does not trigger a
topology change when its link comes and goes; `auto` promotes a port to edge
after three seconds without a BPDU, and demotes it as soon as one arrives. Use
`edge on` for ports where only hosts are attached.

**guard** — `bpdu` disables a port as soon as a BPDU arrives on it (a host port
should never see one); `root` keeps a port from ever becoming the path to the
root, which protects an existing topology from a newly attached bridge that
claims a better priority.

**filter** — the port neither sends nor accepts BPDUs. Useful when the device
on the far side reacts badly to them (some unmanaged switches with loop
prevention cut the link) but you still want STP on the rest of the ports.

## Status

The Spanning Tree page shows the elected root (priority and MAC), the path cost
to it, the root port, the topology-change counter and, per port, the live state
read from the ASIC together with the configured options. The same data is
available as JSON:

```
GET /stp.json
```

The `stp status` command prints the same view on the serial console.

## Limitations

* One spanning-tree instance; no MSTP, no per-VLAN trees.
* No proposal/agreement handshake — an RST-capable neighbour will still
  converge, but through the timers rather than the fast transition.
* Port roles are approximated: the root port and designated ports are
  distinguished, alternate/backup are not.
* Topology changes propagate away from the root only: nothing is announced
  on the root port (no TCN and no BPDUs at all), so bridges upstream rely on
  their own detection.

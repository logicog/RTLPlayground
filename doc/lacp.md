# LACP (Link Aggregation Control Protocol, IEEE 802.3ad)

A static link aggregation group works only if both ends are configured for the
same links, and nothing checks that they are. LACP lets the two ends agree on
which links reach the same partner, aggregates only those, and takes a link out
of the group when the partner stops answering.

The switch runs LACP in software on the 8051. LACP frames belong to the Slow
Protocols, with destination `01:80:C2:00:00:02` and ethertype `0x8809`; the
subtype of LACP frames is 1. The implementation has the Receive, Selection and
Mux machines of 802.3ad clause 43 and does not aggregate links whose partners
differ. Up to `LACP_NUM_LAGS` (4) groups can run LACP; the others stay static.

## Getting the frames to the CPU

While LACP runs, the reserved multicast action for `01:80:C2:00:00:02` is
*trap*, the same mechanism STP uses for its BPDUs, see [stp.md](stp.md). A
trapped LACPDU has no VLAN tag after the RTL tag, and `TRAP_RX_BODY()` gives the
offset of the LACPDU in the received frame. When the last group leaves LACP,
the action goes back to *drop*.

## What has to match across the members

802.3ad requires that every link in an aggregate be full duplex and run at the
same speed. Beyond the standard, anything that changes how a frame is treated
has to be identical across members, because the hash picks the member per frame
and a difference would make the behaviour depend on that choice:

* must match: speed and duplex, VLAN membership and pvid, ingress filtering,
  port isolation
* may differ: EEE, LED configuration, counters, cable diagnostics

None of this is enforced yet — `port_lag_members_set()` writes a member mask and
a hash setting and validates nothing. See #377 for the discussion of whether a
mismatch should be rejected or applied to the whole group.

## Protocol timing

The timer values are in units of the LACP timer, which steps every four ticks
of the system clock, 50 times per second:

| Constant             | Value | Time |
|----------------------|-------|------|
| `LACP_FAST_PERIODIC` | 50    | 1 s  |
| `LACP_SLOW_PERIODIC` | 1500  | 30 s |
| `LACP_SHORT_TIMEOUT` | 200   | 4 s  |

802.3ad specifies a fast periodic time of 1 s with a short timeout of 3 s, and a
slow periodic time of 30 s with a long timeout of 90 s. The switch transmits at
the rate the partner asks for with its TIMEOUT bit. Its own timeout is always
the short one, one second above the standard, so a partner that stops sending
while its link stays up leaves the trunk after 4 s.

The Partner block of a transmitted LACPDU repeats the partner's actor
information as it was last received: system priority, system, key, port
priority, port and state. A Linux partner sets our SYNC only when this block
matches what it sent.

## LACP API

```
void lacp_init(void) __banked;
void lacp_off(void) __banked;
void lacp_lag_set(uint8_t lag, __xdata uint16_t ports) __banked;
void lacp_in(void) __banked;
void lacp_timers(void) __banked;
void lacp_show(void) __banked;
```

`lacp_init()` has to run before the startup configuration replays, because xdata
is not zeroed and the port-to-LAG map uses `0xff` as "no LAG"; a stray zero would
make a port look like it belongs to LAG 0.

## LACP configuration on the serial console

A group is put under LACP by adding `lacp` to the `lag` command:

```
lag 1 lacp 7 8      # ports 7 and 8 become LACP candidates of group 1
lag 1 lacp off      # group 1 leaves LACP, its trunk is cleared (so does "lag 1 lacp")
lag 1 d             # group 1 disabled, whether it ran LACP or not
lag 1 7 8           # plain static aggregation, no protocol
```

A group put under LACP gives up the static trunk it had: the hardware trunk is
cleared and rebuilt from the ports that converge. The other way round, `lag 1 d`
and a static member list take the group out of LACP before they touch the
trunk. A port named in another LACP group moves to the new one. The protocol
starts with the first group put under LACP and stops when the last one is taken
out; there is no separate switch for it. Two commands act on the whole engine:

```
lacp off            # every group leaves LACP, protocol stopped
lacp show
```

`lacp show` prints the engine state, then one line per group with its candidate
ports, the elected aggregator and the trunk members actually programmed into the
hardware, then one line per port with the actor and partner state bytes and the
receive-machine state.

## LACP configuration via the Web Interface

Each group on the LAG page has a Static/LACP mode next to its member ports;
for an LACP group it shows the elected aggregator and the active members. Below
the groups a table has one row per LACP port with its group, actor and partner
state, receive state, LACPDU count and partner system. The page reads
`/lacp.json` every three seconds while it is open. The member checkboxes of a
static group come from the hardware registers in `/lag.json`.

## A test with a Linux 802.3ad bond

Two ports of the switch to two interfaces of one host:

```
ip link add bond0 type bond mode 802.3ad
ip link set enp1s0 down && ip link set enp1s0 master bond0
ip link set enp2s0 down && ip link set enp2s0 master bond0
ip link set bond0 up
```

On the switch, with those two ports as the group:

```
lag 1 lacp 7 8
```

`lacp show` should then give both ports an actor state of `0x3f` (ACTIVITY,
TIMEOUT, AGGREGATION, SYNC, COLLECTING and DISTRIBUTING), a partner state with
SYNC set, and the partner's system MAC. `lag show` lists the trunk members as
programmed in the hardware and should show both ports.

On the host, `/proc/net/bonding/bond0` should show the same `Aggregator ID` for
both slaves; different IDs mean that each link is in a group of its own. Both
churn states should read `none`.

Further checks:

* `tcpdump -i enp1s0 ether proto 0x8809` on each slave shows the LACPDUs of the
  switch and of that slave, never those of the other slave.
* The host log has no `illegal loopback`.
* The three reserved bytes after the actor state in a LACPDU from the switch
  are zero.

When one slave goes down, the group narrows to the remaining member, and the
slave rejoins after it comes back.

## Known limits

* No Churn Detection machines and no Marker protocol.
* Coupled Mux: collecting and distributing are enabled together.
* One aggregator per group.
* Selection compares the partner's system only, not its key: two aggregates
  that one partner system offers on the same group land in one trunk here.
* Member compatibility is not validated; see #377.

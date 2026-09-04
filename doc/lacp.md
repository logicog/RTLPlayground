# LACP (Link Aggregation Control Protocol, IEEE 802.3ad)

A static link aggregation group works only if both ends agree about it in
advance. Nothing checks that they do: cable a member to the wrong neighbour and
the group keeps hashing frames onto a link that goes somewhere else. LACP is the
protocol that makes the two ends agree, and keeps agreeing — it discovers which
links reach the same partner, aggregates only those, and drops a link out of the
group when the partner stops answering.

The switch runs LACP in software on the 8051. Frames are the Slow Protocols
group: destination `01:80:C2:00:00:02`, ethertype `0x8809`, subtype 1. The
implementation covers the parts that decide whether an aggregate forms:
the Receive machine and the Selection and Mux machines of 802.3ad clause 43,
`recordPDU` and `update_NTT`, and the mis-cabling protection that refuses to
aggregate links whose partners differ. It does not implement the Churn Detection
machines or the Marker protocol, and the Mux machine is the coupled variant, so
collecting and distributing are enabled together rather than in sequence.

Up to `LACP_NUM_LAGS` (4) groups can be placed under LACP; the rest stay static.

## Getting the frames to the CPU

This is the part that took the longest and is worth writing down, because the
obvious route does not work on this chip.

Reserved multicast addresses are handled by the RMA block before the L2 lookup
happens. Each address has its own register, `RMA0_CONF + index*4`, and the
action field sits in bits [5:4]: 0 forward, 1 trap to CPU, 2 drop, 3 forward
excluding the CPU.

```
#define RTL837X_RMA0_CONF	0x4ecc	/* 01:80:C2:00:00:00, +4 per address */
#define RTL837X_RMA2_CONF	0x4ed4	/* 01:80:C2:00:00:02, Slow Protocols */
#define RTL837X_RMA_ACT_FORWARD	0x00000000
#define RTL837X_RMA_ACT_DROP	0x00000020
```

`TRAP` looks like the right action and is not. On these boards its destination
is an *external* CPU on a physical port (`cpuTag_externalCpuPort_set`,
`EXT_CPU_CTRL 0x6724`), which the hardware does not have, so trapped frames are
taken out of the forwarding path and never arrive anywhere. The receive counters
stay at zero. The same is true of an ACL rule with `FWD_INT_TRAP` or a redirect
aimed at the CPU port: the rule matches, the frames leave the normal path, and
nothing reaches the 8051.

What does work is `FORWARD`, because the CPU port is in the forwarding domain,
so a forwarded frame lands in the NIC receive ring the firmware already polls.
On its own that also floods the frame to every other port in the VLAN, which
leaks a neighbour's LACPDUs to hosts that must not see them and, with two bonds
on one switch, poisons both. So the forward is constrained: `lacp_setup()`
writes a **static L2 multicast entry for `01:80:C2:00:00:02` whose member mask
is the CPU port only**, one entry per distinct pvid among the candidate ports.
The forward lookup hits that entry instead of the VLAN flood mask, so the frames
reach the CPU and nowhere else.

The entries are written through the table access port (`ITA_CTRL0 0x5CAC`), the
same one the L2 and VLAN tables use, and show up on the L2 page as static
entries on the CPU port, so the containment can be checked from the web UI.
Lookups are IVL on this chip, which is why there is one entry per pvid rather
than a single VID 0 entry — a VID 0 entry is never matched.

When LACP is switched off the action goes to `DROP`, which acts before the L2
lookup, so the static entries left behind are inert.

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

```
#define LACP_FAST_PERIODIC	0x0032	/* fast TX ~1 s, worst ~1.6 s; partner expires at 3 s  */
#define LACP_SLOW_PERIODIC	0x05dc	/* slow TX ~30 s, worst ~48 s; partner expires at 90 s */
#define LACP_SHORT_TIMEOUT	0x0300	/* we drop a silent partner after 6-9 s  */
#define LACP_LONG_TIMEOUT	0x5a00	/* long-timeout variant, after 3-5 min   */
```

One unit is one call of `lacp_timers()`, measured at 50 Hz on a SWTGW218AS: 50
units came out as 1.00 s on the wire in all 116 intervals of a two minute
capture.

The two transmit periods are worth a word, because they are the one thing here
that is not ours to choose freely. Our own timeouts only decide how patient we
are with a partner that has gone quiet, and being slow there costs nobody
anything. The transmit period is the opposite: it decides when the *partner*
gives up on us. 802.3ad pairs a 1 s fast period with a 3 s short timeout, and a
30 s slow period with a 90 s long timeout, and the ratio is the whole point.

Getting that wrong does not break the aggregate, which is what makes it easy to
miss. An earlier version of this code transmitted every 2 to 3 s while the
partner was asking for the fast rate. The links still aggregated and still
carried traffic, but every time a gap crossed 3 s the partner's receive machine
timed us out and recovered: 13 of its 93 LACPDUs came back carrying `EXPIRED`
and reporting our SYNC as cleared. Since a received actor state is recorded
verbatim as the partner state and sent back out, the flap was visible from both
ends, and the bond's churn machines never settled. With the periods above, the
same capture shows no expiry at all and both churn states read `none`.

## LACP API

```
void lacp_init(void) __banked;	/* boot init: clear per-LAG state */
void lacp_setup(void) __banked;
void lacp_off(void) __banked;
void lacp_cmd(uint8_t on) __banked;	/* "lacp on|off" master engine handler */
void lacp_lag_set(uint8_t lag, uint16_t ports) __banked;
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
lag 1 lacp off      # group 1 goes back to static (so does "lag 1 lacp")
lag 1 7 8           # plain static aggregation, no protocol
```

The engine itself is switched separately, so a configuration can be prepared
before the protocol starts:

```
lacp on
lacp off
lacp show
```

`lacp show` prints the engine state, then one line per group with its candidate
ports, the elected aggregator and the trunk members actually programmed into the
hardware, then one line per port with the actor and partner state bytes and the
receive-machine state.

## LACP configuration via the Web Interface

The LAG page has an LACP section: the engine toggle, and a table with one row
per port showing its group, actor and partner state and receive state. It reads
`/lacp.json` every two seconds. Trunk membership itself is shown by the existing
LAG table, which reads the hardware registers, so the two can be compared.

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
lacp on
```

`lacp show` should then give both ports an actor state of `0x3f`, which is
ACTIVITY, TIMEOUT, AGGREGATION, SYNC, COLLECTING and DISTRIBUTING together, a
partner state with SYNC in it, and the partner's system MAC. `lag show` should
list both ports as trunk members, since that reads the hardware rather than the
protocol's opinion of itself.

The host's view is the one worth trusting, in `/proc/net/bonding/bond0`. Both
slaves want the **same `Aggregator ID`**; two different numbers mean the bond
has not aggregated anything and each link is sitting in its own group, which
looks healthy at a glance and is not. Both churn states should read `none`.
`monitoring` that never settles is the signature of our transmit period being
too slow for the rate the partner asked for.

Three things are worth checking past convergence, because each of them has been
broken here at some point:

* `tcpdump -i enp1s0 ether proto 0x8809` on each slave should show the switch's
  LACPDUs and that slave's own, and never the other slave's. A sibling leak is
  what the FDB steering exists to prevent.
* No `illegal loopback` in the host's log.
* In a hex dump of a frame from the switch, the three reserved bytes after the
  actor state should be zero. They used to carry whatever the transmit buffer
  held from the previous frame.

Pulling one slave down and putting it back is the cheapest failure test. The
group narrows to the surviving member and comes back within tens of seconds,
and traffic across the transition loses a few percent at the edges rather than
half of everything, which is what it would lose if the ASIC kept hashing onto
the dead member.

## Known limits

* No Churn Detection machines and no Marker protocol.
* Coupled Mux: collecting and distributing are enabled together.
* One aggregator per group.
* The static FDB steering entries are written at configuration time, so
  changing a port's pvid while LACP is running needs `lacp off` and `lacp on`
  to refresh them.
* Member compatibility is not validated; see #377.

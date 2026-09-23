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

Reserved multicast addresses are handled by the RMA block before the L2 lookup
happens. Each address has its own register and the action field sits in bits
[5:4]: 0 forward, 1 trap to CPU, 2 drop, 3 forward excluding the CPU. The
registers are packed and consecutive by *entry* rather than by address byte, so
the step of 4 counts entries, not bytes of the address: `...:00` sits at
`0x4ecc` and `...:02` at `0x4ed4`, but `...:08` sits at `0x4ee0`.

```
#define RTL837X_RMA0_CONF	0x4ecc	/* 01:80:C2:00:00:00, entry 0 */
#define RTL837X_RMA2_CONF	0x4ed4	/* 01:80:C2:00:00:02, Slow Protocols */
#define RTL837X_RMA_ACT_MASK	0x30
#define RTL837X_RMA_ACT_TRAP	0x10
#define RTL837X_RMA_ACT_DROP	0x20
```

The engine puts this address on *trap* while it runs. A trap takes the frame
out of the forwarding path and hands it to the port named in `EXT_CPU_CTRL`
(`0x6724`), whose reset value names no port at all: the destination is meant to
be an *external* CPU on a physical port, which these boards do not have. That
is why the trap first looked like a dead end - the receive counters stayed
frozen while the partner kept sending, and the ingress discard counter grew
instead. `trap_init()` points the register at the CPU port once at startup, and
the same frames then arrive.

`FORWARD` reaches the CPU too, because the CPU port is in the forwarding
domain, but a forwarded frame is subject to the L2 lookup and on its own floods
to every other port in the VLAN. That leaks a neighbour's LACPDUs to hosts that
must not see them and, with two bonds on one switch, poisons both; a reserved
link-local group must not be forwarded at all. Containing it takes a static L2
multicast entry with a CPU-only member mask, and because lookups are IVL on
this chip, one entry per distinct pvid over all front panel ports - a VID 0
entry is never matched, and a port outside the LACP groups needs its pvid
covered as well, or a LACPDU arriving there misses the lookup and is flooded
through its VLAN.

The trap needs none of that and reaches further, because it acts on the address
before the VLAN lookup and before the ingress filters. An untagged LACPDU is
delivered on a port that admits tagged frames only, where a forwarded one is
dropped by the acceptable-frame-type filter; and it is delivered on a port STP
holds in blocking, so a group converges without waiting out the forward delay.
Both were measured against forward on the same ports of an SWTGW218AS.

A trapped frame arrives without the VLAN tag the hardware inserts into a
forwarded one, so the LACPDU body sits four bytes earlier. `TRAP_RX_BODY()`
gives the offset of the body for either form, and `lacp_in()` reads the PDU
from there.

Neighbouring addresses in the same block still differ in other respects -
`...:00` is exempt from the ingress STP-state check and `...:02` is not, which
the trap makes moot only because it acts before that check - so measure before
carrying a conclusion about one of them across to another.

When LACP is switched off the action goes to `DROP`, which acts before the L2
lookup, so there is nothing to undo.

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
#define LACP_FAST_PERIODIC	0x0032	/* fast TX 1 s; partner expires at 3 s   */
#define LACP_SLOW_PERIODIC	0x05dc	/* slow TX 30 s; partner expires at 90 s */
#define LACP_SHORT_TIMEOUT	0x00c8	/* silent partner dropped after 4 s   (802.3ad: 3 s)  */
#define LACP_LONG_TIMEOUT	0x1770	/* long-timeout variant, after 120 s  (802.3ad: 90 s) */
```

One unit is one step of the LACP timers, taken every four system ticks, so 50
per second whatever the pace of the main loop; 50 units came out as 1.00 s on
the wire in all 116 intervals of a two minute capture.

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

Our own timeouts sit one period above the standard's 3 s and 90 s, which
absorbs a single lost LACPDU plus the tick jitter. A partner whose LACP has
stopped while its link stayed up is out of the trunk after that; anything
longer keeps hashing traffic onto a member that no longer agrees to carry it.

## LACP API

```
void lacp_init(void) __banked;	/* boot init: clear per-LAG state */
void lacp_off(void) __banked;
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

A group put under LACP gives up the static trunk it had: the hardware trunk is
cleared and rebuilt from the ports that converge. A port named in another LACP
group moves to the new one. The protocol starts with the first group put under
LACP and stops when the last one is taken out; there is no separate switch for
it. Two commands act on
the whole engine:

```
lacp off            # every group back to static, protocol stopped
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
  what the trap prevents: the PDU never enters the forwarding path at all.
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
* Member compatibility is not validated; see #377.

# Spanning Tree (STP / RSTP / MSTP)

The switch can take part in a spanning tree (IEEE 802.1D / 802.1w) so that
redundant links between bridges are blocked instead of forming a loop. The
implementation elects a root bridge from the BPDUs it receives, picks the
cheapest path to it as the root port, blocks ports on which a better bridge is
already designated, promotes ports to forwarding once their listen period
expires, drops information a neighbour stopped sending, and blocks a port on
which it sees its own BPDU. With MSTP (IEEE 802.1Q) groups of VLANs get
spanning trees of their own inside an MST region.

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

802.1D also requires `2 * (fwd - 1) >= maxage >= 2 * (hello + 1)`. A
command typed on the console or sent by the web page that would break it is
refused and the value stays as it was, so change the values in an order that
keeps the rule at every step (for a larger max age raise the forward delay
first). The web page shows the error, and a refused command is not recorded
in the command history, so saving the configuration does not pick it up. Lines of the saved configuration are taken one by one whatever their
order, and `stp on` checks the result: if the rule is broken it warns and
starts with 2/20/15 instead.

The values above are what the switch uses while it is the root. Otherwise it
takes max age and forward delay from the BPDUs on its root port, as 802.1D
requires, and passes them on in its own BPDUs with the message age one second
higher; the hello time stays its own.

A port entering the tree discards for one forward delay, then learns
addresses without forwarding for a second one, and only then forwards (an
edge port skips the wait). In RSTP mode on a point-to-point link
(`p2p auto` or `on`) it can go faster: a designated port that is still
discarding sets the proposal flag, and forwards as soon as the bridge on the
other side answers with an agreement. When the root port receives a proposal,
the switch first puts its other non-edge designated ports back to discarding
(they propose in turn), then forwards the root port and answers with an
agreement; this sync happens once per root port, later proposals are only
answered. An alternate or backup port that receives a proposal does the same
sync once for the information it holds and answers with an agreement in a
BPDU with the alternate role, so the bridge on the other side does not have
to wait out its timers; it stays discarding itself. An agreement is accepted
from a root or an alternate port on the other side. A new root port forwards at once, after every port that was root within
the last forward delay has been put back to discarding; such a recent root
port stays closed, even to an agreement, until that forward delay is over.
A new root port that was a backup port within the last two hello times does
not take the fast path and waits for its timers.

A designated port that hears worse information from a bridge that still
claims to be designated and learning or forwarding on the same link has a
dispute: usually one direction of the link is lost and the other bridge does
not hear this switch's BPDUs. The port goes back to discarding, and every
further such BPDU restarts its listen period, so it does not forward while
the dispute lasts.

With `stp version rstp` each port still talks 802.1D to a neighbour that
does: once a Config BPDU or TCN arrives and the migrate time (3 s since the
port came up or last switched) is over, the port sends Config BPDUs, uses
the 802.1D TC window and reports changes with TCN if it is the root port. An
RST BPDU after the migrate time switches it back, and so does a link that
comes up again or `stp port <n> mcheck`. Any worse information heard on a designated port, disputed or
not, is answered with a BPDU at once instead of at the next hello, so the
other bridge learns sooner that it is not the designated one. Information heard on a port stays
valid for three of the sender's hello times; a BPDU whose message age has
reached its max age is not used at all. When the root port's information
expires the switch moves to the next best port, or reclaims the root role if
there is none.

Among ports offering the same root, the lowest root path cost wins, then the
designated bridge, the designated port ID, and finally this switch's own port
ID. A worse BPDU from a different bridge does not replace what a port holds;
a worse one from the same designated bridge and port does.

## Topology changes

A topology change is detected when a non-edge port enters forwarding. That
port, and every other forwarding non-edge port, then carries the TC flag in
its BPDUs for hello time plus one second (max age plus forward delay with
`stp version stp`), and each of the other ports flushes the addresses it
learned. The root port takes part too: while its TC window runs it sends
BPDUs toward the root, so the change reaches the rest of the network. With
`stp version stp` the root port sends TCN instead, until a Config BPDU with
TCA comes back.

A TC flag received on a root or designated port is passed on the same way to
the other ports, not back to the port it came in on. A legacy TCN is
acknowledged with TCA and then treated like a received TC. A port that turns
alternate, loses its link, or is blocked for a loop flushes its own addresses
without raising a topology change, and an edge port never raises one.

## Bridge settings

```
stp prio <0-15>         # bridge priority = n * 4096, default 8 (32768)
stp version rstp|stp    # RST BPDUs (default) or legacy Config BPDUs
stp hello|maxage|fwd|txhold <seconds>
stp pathcost long|short # automatic port costs, 802.1D-2004 (default) or 802.1D-1998
stp bpdu flood|filter   # BPDUs while STP is off: pass them on (default) or drop them
```

The bridge with the lowest priority wins the root election; ties are broken by
the MAC address. If you do not want this switch to become the root of an
existing network, give it a worse priority than the current root — `stp prio 15`
(61440) is the usual "never me" value.

A port without a configured cost takes it from its link speed. The long
method uses the 802.1D-2004 values (20000 for 1G, 2000 for 10G); the short
one the 802.1D-1998 table for bridges that still use 16-bit costs (100 for
10M, 19 for 100M, 4 for 1G, 2 for 10G, and 3 for 2.5G and 5G, which that
table does not list). All bridges in a network should use the same method.

`stp bpdu filter` keeps BPDUs from being passed between ports while STP is
off, instead of the default flooding that lets a surrounding spanning tree
run through the switch. It takes effect at once when STP is off, otherwise
when STP is turned off.

## Per-port settings

```
stp port <1-9> on|off              # take part in STP, or stay plain forwarding
stp port <1-9> edge on|off|auto    # host-facing port handling (default: auto)
stp port <1-9> cost <0-200000000>  # path cost, 0 = automatic (20000)
stp port <1-9> prio <0-240>        # port priority, steps of 16
stp port <1-9> guard none|bpdu|root
stp port <1-9> filter on|off       # neither send nor accept BPDUs
stp port <1-9> p2p auto|on|off
stp port <1-9> mcheck             # try RSTP again on a port that fell back to STP
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

## Link aggregation

A LAG (Link Aggregation Group) is one port of the tree, with its own roles,
timers and states. The switch supports up to 4 LAGs; each one takes the place
of its member ports in the STP status and gets its own row on the Spanning
Tree page. A group carries its own path cost, priority, edge, guard and
point-to-point settings, and takes the same commands as a port:

```
stp lag <1-4> on|off|mcheck|edge|cost|prio|guard|filter|p2p ...
stp lag 1 cost 10000    # the group decides, not its members
```

LAGs are configured with the `lag` command. Once a port is a member of a LAG
it can no longer be configured on its own for STP, and `stp port <n>` on a
member names the group to configure instead. Membership is read from the
aggregation registers once a second, so a group changing under LACP is picked
up without any coordination between the two; a port that joins or leaves a
group, and the group itself, start over from discarding. The automatic path
cost of a group is the one of its lowest member that has a link.

The hardware keeps no STP state for a LAG as a whole, so every state change is
written to all member ports, in a single register write. BPDUs go out through
the lowest member that has a link and carry the group's own port id, and a
BPDU received on any member belongs to the group. Losing one member of a live
LAG is not a topology change; the group only goes down with its last link.

## MSTP

`stp version mstp` runs the Multiple Spanning Tree Protocol of IEEE 802.1Q.
Bridges with the same region name, revision and VLAN to instance table form
an MST region. Inside it every instance with VLANs has a spanning tree of its
own, so the VLANs of different instances can use different links. Towards
bridges outside the region, RSTP and STP bridges included, the whole region
behaves as one bridge of the common spanning tree (CIST).

```
stp version mstp
stp region lab                 # up to 32 characters, no spaces
stp revision 1
stp msti 1 vlan 10-19,100      # the VLANs of instance 1; none gives them back
stp msti 2 vlan 20-29
stp msti 1 prio 1              # bridge priority in instance 1, times 4096
stp port 9 msti 2 cost 2000    # port path cost in instance 2, 0 = automatic
stp port 9 msti 2 prio 64      # port priority in instance 2
stp maxhops 20                 # 6-40, how far information travels in the region
stp mstp                       # region, hop count, digest, VLANs of each instance
stp msti 1                     # the tree of instance 1
```

Instances 1 to 15 are supported, as many as the switch keeps port states for.
A VLAN not given to an instance belongs to the CIST, and an instance without
VLANs does not run. The configuration digest, which tells one region from
another, is worked out in full when STP starts in MSTP mode and on every
change of the table, so a BPDU never carries a digest that describes a
different table than the one in the switch. That takes 132 MD5 blocks, in
the order of a third of a second on this CPU; a change made while STP is off
is finished in the background once it runs.

A port whose neighbour is in the same region is internal. It carries the
instance trees with their own roles, states, proposals, agreements and
topology changes, and the remaining hops count limits how far information
travels instead of the message age. A port towards another region, or towards
an RSTP or STP bridge, is a boundary port: every instance takes the CIST role
and state there, and the CIST root port shows as a master port of each
instance.

The switch keeps a port state register for every instance
(`RTL837X_MSTP_STATES + 4 * instance`) and the instance of a VLAN in bits
20-23 of its VLAN table entry. The firmware writes the instance of every VLAN
when STP starts in MSTP mode, when the table changes while it runs and when a
VLAN is created, and gives every VLAN back to the CIST when STP stops or
leaves MSTP. Switching to or from MSTP while STP runs restarts it; switching
between STP and RSTP does not.

The region name, revision and hop count are with the bridge settings of the
Spanning Tree page, and its MST instances card lists the VLANs of each
instance and shows its tree. `stp status`, `stp mstp` and `stp msti <n>` only
show state and are not kept in the command history, so a page that polls them
does not push saved settings out of it.

## Status

The Spanning Tree page shows the elected root (priority and MAC), the path cost
to it, the root port, the topology-change counter and the time since it last
moved, the max age and forward
delay in use (the root's, when this switch is not the root) and, per port, the
live state read from the ASIC, or "No link", together with the configured
options. For a port taking part in the tree it also shows the designated
bridge, port and cost, and whether it talks RSTP or 802.1D to its neighbour;
the Check button runs `stp port <n> mcheck`. The same data is available as
JSON:

```
GET /stp.json
```

The Counters section of the same page (or `/stpcnt.json`) shows per port the
BPDUs received and sent, those of them that carried the topology change flag
(TCNs included), and the time since the last BPDU arrived. The counters start
from zero with `stp on`, and `stp clear` resets them; the topology change
counter is not reset.

The `stp status` command prints the same view on the serial console. A port
is reported with one of the roles Root, Designated, Alternate, Backup or
Disabled; Disabled covers a port with STP turned off on it, one shut down by
BPDU guard and one without link.

BPDUs shorter than their type allows, judged by the 802.3 length field, are
dropped: 7 bytes for a TCN, 38 for a Config BPDU and 39 for an RST BPDU,
LLC header included.

## Limitations

* No per-VLAN trees; MSTP groups VLANs into at most 15 instances.
* Edge, guard, filter and point-to-point settings belong to the port and
  apply in every instance; path cost and priority can be set per instance.
* A backup port, one that hears the BPDUs of another port of this switch,
  is blocked and reported as Backup, but keeps sending BPDUs: loop detection
  on that segment relies on them.

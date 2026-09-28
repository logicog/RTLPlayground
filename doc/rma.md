# Reserved multicast (RMA)

Frames to the reserved link-local addresses `01:80:C2:00:00:xx`, and to two
Cisco addresses, get an action of their own instead of the usual forwarding.
The `rma` command sets that action and a few flags for each address the switch
knows, the priority of trapped frames, and what happens to PTP frames.

```
rma show
rma <entry> [forward|trap|drop|nocpu] [storm|keep|vlanleak|isoleak on|off]...
rma priority <0-7>
rma lldpmatch on|off
rma ptp priority <0-7>
rma ptp cpu <0-3>
rma ptp <port> [eth2|udp forward|trap|drop|nocpu] [delay|pdelay|asm on|off]...
```

`rma show` prints every setting as the command that sets it, so its output can
be pasted into the configuration as it is. The configuration keeps one line per
entry, and a new line for an entry replaces the old one, so give the whole
entry on one line.

Example, keep LLDP link-local instead of flooding it to all ports, and exempt
BPDUs from storm control:

```
rma 0e trap
rma 00 storm off
```

## Entries

| entry | address | used by |
|---|---|---|
| `00` | 01:80:C2:00:00:00 | STP, RSTP, MSTP BPDUs |
| `01` | 01:80:C2:00:00:01 | PAUSE frames |
| `02` | 01:80:C2:00:00:02 | slow protocols: LACP, marker, OAM |
| `03` | 01:80:C2:00:00:03 | 802.1X (EAPOL), LLDP to the nearest non-TPMR bridge |
| `04` | 01:80:C2:00:00:04 | reserved |
| `08` | 01:80:C2:00:00:08 | provider bridge (802.1ad) STP |
| `0d` | 01:80:C2:00:00:0D | provider bridge MVRP |
| `0e` | 01:80:C2:00:00:0E | LLDP to the nearest bridge, PTP peer delay |
| `10` | 01:80:C2:00:00:10 | all LANs bridge management |
| `11`, `12` | 01:80:C2:00:00:11, 12 | load server, loadable device |
| `13` | 01:80:C2:00:00:13 | IEEE 1905.1 |
| `18`, `1a` | 01:80:C2:00:00:18, 1A | all manager stations, all agent stations |
| `20` | 01:80:C2:00:00:20 | GMRP, MMRP |
| `21` | 01:80:C2:00:00:21 | GVRP, MVRP |
| `22` | 01:80:C2:00:00:22 | other GARP applications |
| `cdp` | 01:00:0C:CC:CC:CC | CDP, VTP, DTP, PAgP, UDLD |
| `csstp` | 01:00:0C:CC:CC:CD | Cisco PVST+ |
| `lldp` | LLDP by EtherType 88cc | used instead of the address entries while `lldpmatch` is on |

The actions:

| action | effect |
|---|---|
| `forward` | forward like any other multicast, which includes the CPU port |
| `trap` | deliver to the CPU only |
| `drop` | drop |
| `nocpu` | forward, but not to the CPU |

and the flags, all off after reset:

| flag | effect |
|---|---|
| `storm off` | the frames are not counted or limited by storm control |
| `keep` | leave with the C-tag format they came with |
| `vlanleak` | forward across VLANs |
| `isoleak` | forward across port isolation |

After reset `01` drops and the others forward, as read on a switch for `00` to
`04`, `0d`, `0e` and `10`. While STP runs it keeps `00` at `trap` and puts it
back to `forward` when it stops, so `rma` refuses to change the action of `00`
while STP is on; its flags can still be set.

A trapped frame goes to the port in `EXT_CPU_CTRL`, which the firmware sets to
the CPU port at startup, see CpuPort.md. `rma priority` is the priority the
trapped frames get on the way to the CPU; a high value keeps BPDUs and LACPDUs
from being lost when the CPU port is busy.

## PTP

PTP frames are handled per ingress port, separately for PTP over Ethernet
(EtherType 88f7, `eth2`) and over UDP (ports 319 and 320, `udp`). The action
only applies to the message types selected for that port:

| flag | message types |
|---|---|
| `delay` | Delay_Req |
| `pdelay` | Pdelay_Req |
| `asm` | Announce, Signaling, Management |

Sync and Follow_Up are never selected. Selected messages are forwarded, dropped
or trapped; a trap goes to the CPU only, and `rma ptp cpu` has to name a CPU
(1, 2 or 3 all reach it here), since with 0 a trapped frame is lost.
`rma ptp priority` is the priority of trapped PTP frames.

## Measured

On a SWTGW218AS, 200 frames to 01:80:C2:00:00:04 from port 4, counted as
multicast frames leaving the CPU port and the uplink:

| setting | CPU | uplink |
|---|---|---|
| `forward` | 200 | 200 |
| `trap` | 200 | 0 |
| `drop` | 0 | 0 |
| `nocpu` | 0 | 200 |

With an ACL rule on the same frames, the ACL decides whenever it names where
the frame goes, and otherwise the RMA action stands:

| RMA | ACL | CPU | uplink |
|---|---|---|---|
| `trap` | `drop` | 0 | 0 |
| `drop` | `permit` | 0 | 0 |
| `drop` | `redirect 9` | 0 | 200 |
| `trap` | `copy 9` | 200 | 200 |
| `forward`, `nocpu`, `drop` | `trap` | 200 | 0 |
| `drop` | `cpu` | 200 | 0 |

`permit` keeps the forwarding decision, and the RMA action is part of it.

`rma lldpmatch on` makes the `lldp` entry decide for LLDP frames (EtherType
88cc) sent to 01:80:C2:00:00:00, 03 and 0E, the three LLDP addresses, in place
of the entries of those addresses. Other EtherTypes to these addresses still
follow their entries, and LLDP to other addresses is not affected.

`storm off` works: with multicast storm control at 10 packets per second on
the ingress port, 21 of 200 frames got through, and all 200 with `storm off`.
`isoleak on` lets the frames past port isolation: with the port isolated to
one other port, none reached the CPU or the uplink, and all 200 with `isoleak
on`.
`vlanleak on` forwards past VLAN membership: frames tagged with a VLAN whose
only front port member was the ingress port reached no other port with
`vlanleak off`, and all 200 reached the uplink, which is not a member, with
`vlanleak on`. The CPU port got them either way, being a member of every VLAN.

`keep on` leaves the tag as received: frames to 01:80:C2:00:00:10 that came in
on the uplink with a VLAN 2 tag left an untagged member port untagged with
`keep off` and tagged with `keep on`.

The trapped frames carry the priority in the low three bits of the first flags
byte of the RTL tag, which follows the reason code (CpuPort.md): 0, 5 and 7
with `rma priority` 0, 5 and 7, and the same for `rma ptp priority`, whose
trapped frames come with reason 0x64. An ACL `priority` action does not change
that byte for an ACL trap (reason 0x67).

PTP, as measured with valid PTP headers from one port:

| setting | selected messages, CPU / uplink |
|---|---|
| `forward` | 200 / 200 over Ethernet, 0 / 200 over UDP |
| `drop` | 0 / 0 |
| `trap`, `rma ptp cpu 1` to `3` | 200 / 0 |
| `trap`, `rma ptp cpu 0` | 0 / 0 |

Frames that are not selected are forwarded whatever the action.

## How it was measured

Every setting above was measured. The switch the measurements went through
filters 01:80:C2:00:00:0x, so all counts are from the switch's own counters and
the hosts on its ports.

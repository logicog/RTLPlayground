# ACL

The switch matches frames against up to 64 rules and can drop them, send them
elsewhere, copy them, count them, police them, retag them or remark their
priority.

```
acl <1-64> [not] <match>... <action>... [<port>...]
acl <1-64> and <match>... [<port>...]
acl <1-64> off
acl meter <0-63> <rate> kbps|pps <burst>
acl field <0-15> off|raw|llc|ipv4|arp|ipv6|ip|l4 <offset>
acl default permit
acl default drop <port>...
acl counter <0-31> bytes|packets
acl counter reset
acl show
```

A frame has to match every field a rule names. The ports after the actions are
the ingress ports the rule applies to; without them it applies to all front
panel ports. `acl <n>` replaces rule `n` and `acl <n> off` removes it; rules,
meters, fields, counter modes and the default are kept in the saved
configuration like any other command.

Examples:

```
acl 1 ethertype 86dd drop 3
acl 2 smac 02:00:00:00:00:01 dmac 02:00:00:00:00:02 mirror 5
acl 3 udp dport 53 dscp 46 priority 6
acl 4 sip 192.168.1.0/24 dport 1000-2000 drop 3 4
acl 5 dmac 00:11:22:33:44:55 permit
acl 6 drop 5
acl 10 dmac 00:11:22:33:44:55 count 1
acl 11 and udp dport 5000
```

## Matching

| match | field |
|---|---|
| `dmac <mac>[/<mask>]`, `smac <mac>[/<mask>]` | MAC address, `aa:bb:cc:dd:ee:ff` |
| `ethertype <hex>[/<hex>]` | EtherType; for a tagged frame the type after the tag, for LLC/SNAP the SNAP type |
| `vlan <vid>[-<vid>]` | VID of the C-tag; frames without a C-tag never match |
| `pri <0-7>` | priority of the C-tag |
| `svlan <vid>[-<vid>]` | VID of the S-tag |
| `tagged`, `untagged` | the frame carries a C-tag or not |
| `pppoe` | PPPoE session frame |
| `nonip`, `arp`, `ipv4`, `ipv6` | layer 3 format |
| `tcp`, `udp`, `icmp`, `igmp` | layer 4 protocol, IPv4 or IPv6 (`icmp` includes ICMPv6) |
| `sip <ip>[/<len>]`, `sip <ip>-<ip>` | IPv4 source address, prefix or range; `dip` the same for the destination |
| `sip6 <hex>[-<hex>]`, `dip6 ...` | lowest 32 bits of an IPv6 address, as a range |
| `proto <0-255>` | IPv4 protocol |
| `tos <hex>[/<hex>]` | IPv4 ToS byte |
| `sport <port>[-<port>]`, `dport ...` | TCP or UDP port, IPv4 or IPv6 |
| `field <0-15> <hex>[/<hex>]` | 16 bits picked by field selector n, see below |

`not` inverts the match of the whole rule. A frame goes through all rules at
once; the lowest numbered rule that matches and has an action of a given kind
decides that kind, independently for each kind. A `permit` rule therefore
protects a frame from a later `drop` rule, while a rule that only counts or sets
a priority does not.

The hardware compares a rule against one of five templates of eight 16-bit
fields, and the firmware picks the first template that holds everything a rule
asks for. An exact `sip`, `dip`, `vlan`, `sport` or `dport` can also be turned
into a range entry, of which there are 16 of each kind (VLAN, IP, L4 port).
Fields that share no template are joined with `and`: `acl <n+1> and <match>`
has no actions of its own and adds its match to rule `n`, so both have to match.
It may continue with further `and` rules.

## Actions

| action | effect |
|---|---|
| `drop` | drop |
| `permit` | forward as usual; overrides later forwarding rules |
| `<port>` or `redirect <port>[,<port>...]` | send to these ports instead |
| `copy <port>[,...]` | forward as usual and also send to these ports |
| `mirror <port>[,...]` | forward as usual and mirror to these ports |
| `isolate <port>[,...]` | forward only to these of the usual ports |
| `cpu` | hand the frame to the 8051 instead of forwarding it |
| `trap` | trap the frame to the 8051 |
| `setvlan <vid>` | classify an untagged frame into this VLAN |
| `outvlan <vid>` | use this VID in the tag on the way out |
| `tag`, `untag`, `keeptag`, `keepremark` | leave with a C-tag, without one, as received, or as received with the priority remarked |
| `priority <0-7>` | internal priority |
| `pcp <0-7>` | remark the 802.1p priority of the outgoing tag |
| `dscp <0-63>` | remark the DSCP of an IPv4 frame |
| `police <0-63>` | police with meter n |
| `count <0-31>` | count in counter n |
| `interrupt` | raise the ACL interrupt |
| `bypass storm\|stp\|vlan` | skip storm control, the STP source check or the ingress VLAN filter |

A rule takes at most one action of each row group: one forwarding action, one
remark, `police` or `count`. The single `<port>` form must be the first action.

A redirect ignores the VLAN membership of the target port: a frame from a VLAN
the port does not belong to still leaves through it, without a VLAN tag, and
the ingress VLAN filter does not stop it. It does respect the spanning tree
state: a rule whose target port is blocked drops the frame. A redirect to a
member of a link aggregation group is spread over the group by the group's hash
and never goes back into the group it came from.

## Meters, counters and field selectors

`acl meter <n> <rate> kbps <burst>` limits to `rate` kbit/s with a bucket of
`burst` bytes, `pps` to `rate` packets per second with a bucket of `burst`
packets. A bucket of 0 passes nothing. Several rules may share a meter.

Counters count packets unless their pair (0-1, 2-3, ...) is switched to bytes.
They are shown with `acl show` and cleared with `acl counter reset`; reading
does not clear them.

A field selector picks 16 bits at `offset` bytes into a part of the frame:
`raw` from the start of the frame after its VLAN tags, `llc`, `ipv4` and `ipv6`
from the start of that header, `arp`, `ip` from the IP payload and `l4` from the
TCP or UDP payload. `acl field 0 ipv6 8` makes `field 0` the first 16 bits of the
IPv6 source address.

## Default action

Frames that match no rule are forwarded. `acl default drop <port>...` drops them
on these ports instead, which turns the rules into a list of what may enter:
any rule without a forwarding action, even one that only counts, lets its
frames through. Management traffic arriving on such a port needs its own rule.

## Hardware notes

Each rule is one entry of the 96-entry rule table: a care and a data half of
five words, fields in words 0-3 and rule information in word 4 (template 0-2,
C-tag/S-tag/PPPoE 3-5, layer 3 format 6-7, layer 4 format 8-10, ingress ports
11-20, valid 21). Rules 65 to 96 are left for the firmware. An entry whose
`ACL_ACT_CTRL` (0x4848 + 4 * n) is zero has no actions and is joined to the
entry before it; the `not` flag is bit 8 of that register.

`ACL_ACT_CTRL` resets to 0xff, which enables every action type; with the action
words at zero the VLAN and policing actions drop every matching frame, so the
firmware enables only the types a rule uses. `ACL_PORT_UNMATCH_PERMIT` (0x481c)
resets to 0, which drops every unmatched frame on a port with ACL enabled; it is
set before `ACL_PORT_EN` (0x4818) when the first rule is added.

Not measured: the S-tag matches and `svlan` ranges need S-VLAN operation, which
the firmware does not configure, and the bypass flags could not be observed
(the redirect already passes the VLAN filter).

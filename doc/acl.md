# ACL

The switch matches frames against up to 64 rules and can drop them, send them
elsewhere, copy them, count them, police them, retag them or remark their
priority.

```
acl <1-64> [not] <match>... <action>... [<port>...]
acl <1-64> and <match>... [<port>...]
acl <1-64> off
acl meter <0-63> <rate> kbps|pps <burst> [ifg]
acl field <0-15> off|raw|llc|ipv4|arp|ipv6|ip|l4 <offset>
acl default permit
acl default drop <port>...
acl counter <0-31> mode bytes|packets
acl counter <0-31> width 32|64
acl counter reset
acl gpio <0-3> on|off
acl gpio polarity high|low
acl show
```

A frame has to match every field a rule names. The ports after the actions are
the ingress ports the rule applies to; without them it applies to all front
panel ports. `acl <n>` replaces rule `n` and `acl <n> off` removes it; rules,
meters, fields, counter modes and the default are kept in the saved
configuration like any other command. A command line holds at most 14 words, so
a long rule is split with `and` (see below).

`acl show` lists the rules in use with the template each one got, the counters
that are not zero, the hardware's hit indicator and the meters exceeded since the
previous `acl show`.

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
| `vlan <vid>[-<vid>]` | VID of the C-tag |
| `pri <0-7>`, `cfi <0-1>` | priority and CFI bit of the C-tag |
| `svlan <vid>[-<vid>]` | VID of the S-tag |
| `spri <0-7>`, `sdei <0-1>` | priority and DEI bit of the S-tag |
| `tagged`, `untagged` | the frame carries a C-tag or not |
| `stagged` | the frame carries an S-tag |
| `pppoe` | PPPoE session frame |
| `nonip`, `arp`, `ipv4`, `ipv6` | layer 3 format |
| `tcp`, `udp`, `icmp`, `igmp` | layer 4 protocol, IPv4 or IPv6 (`icmp` includes ICMPv6) |
| `l4other` | any other IP protocol |
| `sip <ip>[/<len>]`, `sip <ip>-<ip>` | IPv4 source address, prefix or range; `dip` the same for the destination |
| `sip6 <hex>[-<hex>]`, `dip6 ...` | lowest 32 bits of an IPv6 address, as a range |
| `proto <0-255>` | IPv4 protocol |
| `tos <hex>[/<hex>]` | IPv4 ToS byte |
| `sport <port>[-<port>]`, `dport ...` | TCP or UDP port, IPv4 or IPv6 |
| `field <0-15> <hex>[/<hex>]` | 16 bits picked by field selector n, see below |
| `valid <0-15>` | field selector n applies to the frame (its format matches) |

A frame without a C-tag reads its C-tag as all zeroes: `vlan 5` or `pri 3` never
matches it, but `pri 0` and `cfi 0` do, so add `tagged` when that matters.

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
| `copy <port>[,...]` | forward as usual and add these ports |
| `mirror <port>[,...]` | forward as usual and mirror to these ports |
| `isolate <port>[,...]` | forward only to these of the usual ports |
| `cpu` | hand the frame to the 8051 instead of forwarding it |
| `trap [int\|ext\|both]` | trap the frame to the 8051, to the external CPU port, or to both; this firmware makes the 8051 the external CPU port too, so all three reach it |
| `setvlan <vid>` | classify an untagged frame into this VLAN |
| `outvlan <vid>` | use this VID in the tag on the way out |
| `cvidfromsvid` | take the C-VID from the S-tag (not measured) |
| `setsvlan <vid>`, `outsvlan <vid>`, `svidfromcvid` | the same for the S-VLAN |
| `tag`, `untag`, `keeptag`, `keepremark` | leave with a C-tag, without one, as received, or as received but with `pcp` applied |
| `priority <0-7>` | internal priority |
| `pcp <0-7>` | remark the 802.1p priority of the outgoing tag; `keeptag` keeps the received priority instead |
| `dscp <0-63>` | remark the DSCP of an IPv4 frame |
| `police <m>[,<m>[,<m>]]` | police with up to three meters; a frame passes only if every one of them lets it |
| `count <0-31>` | count in counter n |
| `interrupt` | raise the ACL interrupt; the firmware prints `ACL interrupt` once a second while it fires |
| `gpio <0-3>` | pulse ACL GPIO pin n high for each matching frame |
| `bypass storm\|stp\|vlan` | skip storm control, the STP source check or the ingress VLAN filter |

A rule whose only action is `interrupt` or `gpio` drops the frame; together
with any other action, such as `permit` or `count`, it does not.

A rule takes at most one forwarding action, one remark, and `police` or `count`.
The second and third meter of `police` use the rule's C-VLAN and S-VLAN action,
so they do not combine with the VLAN actions of that kind. The single `<port>`
form must be the first action.

A redirect ignores the VLAN membership of the target port: a frame from a VLAN
the port does not belong to still leaves through it, without a VLAN tag, and
the ingress VLAN filter does not stop it. It does respect the spanning tree
state: a rule whose target port is blocked drops the frame. A redirect to a
member of a link aggregation group is spread over the group by the group's hash
and never goes back into the group it came from.

## Meters, counters and field selectors

`acl meter <n> <rate> kbps <burst>` limits to `rate` kbit/s with a bucket of
`burst` bytes, `pps` to `rate` packets per second with a bucket of `burst`
packets; `ifg` also counts the preamble and inter-frame gap of each frame. A
bucket of 0 passes nothing, and a bucket should hold a few frames.
Several rules may share a meter. `acl show` lists which meters have been
exceeded since the previous `acl show` and clears the list.

Counters count packets unless their pair (0-1, 2-3, ...) is switched to bytes,
and are 32 bits wide unless the pair is switched to 64, which joins the two
counters of the pair. They are shown with `acl show` and cleared with
`acl counter reset`; reading does not clear them. Counter numbers stop at 31;
the hardware accepts higher ones but counts them nowhere.

`gpio` drives one of four pins the ACL can take over with `acl gpio <n> on`:
ACL pins 0 to 3 are GPIO 52, 53, 54 and 30. Taking one over removes whatever the
board uses it for; on the SWTGW218AS GPIO 54 is the reset button and GPIO 30 the
SFP module detect, so only pins 0 and 1 are free there. A matching frame pulses
the pin high; `acl gpio polarity` made no difference to that in our test.
doc/gpio.md lists the pins with their other functions.

A field selector picks 16 bits at `offset` bytes into a part of the frame:
`raw` from the start of the frame after its VLAN tags, `llc`, `arp`, `ipv4` and
`ipv6` from the start of that header, `ip` from the IP payload (the layer 4
header, IPv4 or IPv6) and `l4` from the TCP or UDP payload. A selector whose
part the frame does not have is not valid for it, which `valid <n>` tests.
`acl field 0 ipv6 8` makes `field 0` the first 16 bits of the IPv6 source
address. Selectors 4 to 11 share a template, so a whole IPv6 address fits in one
rule and an `and` rule:

```
acl field 4 ipv6 8
...
acl field 11 ipv6 22
acl 1 field 4 fd00 field 5 0000 field 6 0000 field 7 0000 drop
acl 2 and field 8 0000 field 9 0000 field 10 0000 field 11 0001
```

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
entry before it, even when the entry itself is empty: an empty entry with a zero
`ACT_CTRL` after a rule stops that rule from matching anything. Removing a rule
therefore puts `ACT_CTRL` back to its reset value 0xff. The `not` flag is bit 8
of that register.

`ACL_ACT_CTRL` resets to 0xff, which enables every action type; with the action
words at zero the VLAN and policing actions drop every matching frame, so the
firmware enables only the types a rule uses. `ACL_PORT_UNMATCH_PERMIT` (0x481c)
resets to 0, which drops every unmatched frame on a port with ACL enabled; it is
set before `ACL_PORT_EN` (0x4818) when the first rule is added.

## S-VLAN

The S-tag matches and actions need S-VLAN operation, which this firmware does
not configure. Measured with the S-VLAN registers set by hand: a port reads
S-tags only while it is an S-VLAN service port; `stagged`, `svlan`, `spri`,
`sdei` and S-VID ranges then match. `outsvlan` adds an S-tag with
that VID to frames leaving a service port, and `svidfromcvid` forwards them in
the S-VLAN of their C-VID. With `setsvlan` the frames did not arrive, even with
the S-VLAN defined on the target port. Frames that get no S-VID at all are
dropped on the way to a service port.

## Not measured

`bypass stp`, which would need a live port in blocking state; the storm and
VLAN filter bypasses were measured. `acl show` also prints the hardware's hit
indicator, which read zero in every test.

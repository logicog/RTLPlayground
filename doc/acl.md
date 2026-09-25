# ACL

The switch can drop frames, send them to another port, hand them to the CPU or
mirror them, based on the destination MAC, the source MAC and the EtherType.

```
acl <1-64> [dmac <aa:bb:cc:dd:ee:ff>] [smac <aa:bb:cc:dd:ee:ff>] [ethertype <hex>] <action> [<port>...]
acl <1-64> off
```

A rule matches on any combination of the three fields, at least one of them;
a frame has to match all the fields given. The action is one of:

* `drop`
* `cpu`: hand the frame to the 8051 instead of forwarding it, in the same
  format as a forwarded one
* `<port>`: send the frame to that port instead
* `mirror <port>`: forward the frame as usual and send a copy to that port

The ports after the action are the ingress ports the rule applies to; without
them it applies to all front panel ports. For example, to drop IPv6 arriving on
port 3, and to mirror the frames one host sends to another onto port 5:

```
acl 1 ethertype 86dd drop 3
acl 2 smac 02:00:00:00:00:01 dmac 02:00:00:00:00:02 mirror 5
```

When several rules match a frame, the one with the lowest number wins. `acl <n>`
replaces rule `n`, and `acl <n> off` removes it; both are kept in the saved
configuration like any other command. Frames that match no rule are forwarded
as usual. A rule cannot make an exception to a later one: a lower-numbered rule
that only mirrors, or does nothing, does not stop a later `drop` rule.

A redirect to a port ignores the VLAN membership of that port: a frame from a
VLAN the target port does not belong to still leaves through it, without a VLAN
tag. It does respect the spanning tree state: a rule whose target port is
blocked drops the frame. A redirect to a member of a link aggregation group is
spread over the group by the group's hash, like forwarded traffic, and skips a
member that is blocked.

## Hardware notes

All rules use template 0, which holds the destination MAC, the source MAC and
the EtherType (field types 0 to 6, assembled with `ACL_TEMPLATE()`). Each field
a rule names must match exactly, the others are left as don't care; the ingress
ports are restricted through the rule's port field.

`ACL_ACT_CTRL` (0x4848 + 4 * rule) enables the action types of a rule. Its reset
value 0xff enables all of them, and with the action words left at zero the VLAN
and policing actions drop every matching frame on their own. Each rule is
therefore written with only the forward action enabled (0x20). `drop`, `cpu`
and `<port>` use its redirect mode: an empty port mask drops the frame, bit 9
sends it to the CPU. `mirror` uses its mirror mode, which forwards the frame as
usual and adds the given port.

`ACL_PORT_UNMATCH_PERMIT` (0x481c) resets to 0, which drops every frame that
matches no rule on a port with ACL enabled. It is set before `ACL_PORT_EN`
(0x4818) when the first rule is added, and both are cleared again when the last
one goes.

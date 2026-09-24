# ACL

The switch can drop frames, send them to another port, or hand them to the CPU,
based on the destination MAC, the source MAC or the EtherType.

```
acl <1-64> dmac <aa:bb:cc:dd:ee:ff> (drop|cpu|<port>) [<port>...]
acl <1-64> smac <aa:bb:cc:dd:ee:ff> (drop|cpu|<port>) [<port>...]
acl <1-64> ethertype <hex> (drop|cpu|<port>) [<port>...]
acl <1-64> off
```

The ports after the action are the ingress ports the rule applies to; without
them it applies to all front panel ports. `cpu` hands the frame to the 8051 in
the same format as a forwarded one. For example, to drop IPv6 arriving on port 3:

```
acl 1 ethertype 86dd drop 3
```

When several rules match a frame, the one with the lowest number wins. `acl <n>`
replaces rule `n`, and `acl <n> off` removes it; both are kept in the saved
configuration like any other command.

A redirect to a port ignores the VLAN membership of that port: a frame from a
VLAN the target port does not belong to still leaves through it, without a VLAN
tag. It does respect the spanning tree state: a rule whose target port is
blocked drops the frame. A redirect to a member of a link aggregation group is
spread over the group by the group's hash, like forwarded traffic, and skips a
member that is blocked.

## Hardware notes

All rules use template 0, which holds the destination MAC, the source MAC and
the EtherType (field types 0 to 6). A rule matches one of them exactly; the
ingress ports are restricted through the rule's port field.

`ACL_ACT_CTRL` (0x4848 + 4 * rule) enables the action types of a rule. Its reset
value 0xff enables all of them, and with the action words left at zero the VLAN
and policing actions drop every matching frame on their own. Each rule is
therefore written with only the forward action enabled (0x20). The forward
action is a redirect: an empty port mask drops the frame, bit 9 sends it to the
CPU.

`ACL_PORT_UNMATCH_PERMIT` (0x481c) resets to 0, which drops every frame that
matches no rule on a port with ACL enabled. It is set before `ACL_PORT_EN`
(0x4818) when the first rule is added, and both are cleared again when the last
one goes.

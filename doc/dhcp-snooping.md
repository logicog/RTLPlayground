# DHCP snooping

DHCP snooping stops a DHCP server that is plugged into the wrong port from
handing out addresses. Ports are either trusted or untrusted; replies from a
DHCP server (UDP from source port 67) that arrive on an untrusted port are
dropped by the switch hardware, so the 8051 is not involved.

```
dhcp snooping trust <port>...
dhcp snooping trust none
dhcp snooping on
dhcp snooping off
dhcp snooping
dhcp snooping binding
```

`dhcp snooping trust` sets the list of trusted ports, normally the uplink
towards the real DHCP server. Without any argument after `dhcp snooping`, the
command prints the current state. For example, with the DHCP server behind
port 9:

```
dhcp snooping trust 9
dhcp snooping on
```

As long as no port is trusted, `dhcp snooping on` does not filter anything and
says so: every port would be untrusted, including the one the real server is
on. Both commands are kept in the saved configuration, and the order in which
they are replayed at boot does not matter.

Only frames from UDP port 67 are dropped, and only on untrusted ports, so
clients are not affected. A DHCP relay agent also sends from port 67 and has
to sit on a trusted port. The switch's own DHCP client (`ip dhcp`) is behind
the same filter: the port towards its DHCP server must be trusted, or the
lease will not be renewed. DHCPv6 is not covered.

## Bindings

While snooping is on, the switch also keeps a table of the addresses the DHCP
server handed out, `dhcp snooping binding` lists it:

```
02:00:00:00:00:c1 10.99.0.50 vlan 2 port 4 lease 118 s
```

A request from a client on an untrusted port creates an entry with the
client's MAC address, VLAN and port, shown as `pending`. An ACK from a trusted
port fills in the address and the lease time, and the entry is removed when the
lease runs out, on a NAK, a RELEASE or a DECLINE, or when a pending entry sees
no ACK within 60 seconds. The remaining lease is shown in seconds, minutes,
hours or days, and as `infinite` when the ACK carries no lease time. The table
holds 16 entries; a request from a further client is not recorded until an
entry is free. It is not saved, and turning snooping off clears it. It is the groundwork for IP source guard and dynamic
ARP inspection, which do not exist yet.

## Hardware notes

Snooping uses three rules of the ACL rule table, numbers 65 to 67, just past
the 64 rules the `acl` command uses (see acl.md). A user rule that matches the
same frames and has a forwarding action therefore takes precedence. All three
match IPv4 UDP frames by their source port:

| rule | ingress | source port | action |
|---|---|---|---|
| 65 | untrusted | 67 | drop |
| 66 | trusted | 67 | copy to the CPU |
| 67 | untrusted | 68 | copy to the CPU |

A copy leaves the normal forwarding of the frame alone. It is needed because a
server usually answers a client with a unicast frame the CPU would not see, and
because the CPU is not a member of every VLAN. Copies of frames addressed to
another MAC address do not reach the switch's own IP stack; broadcast DHCP
replies do, and its DHCP client ignores those with a transaction ID that is not
its own.

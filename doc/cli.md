# CLI 

| Old Command | New Command |
| ---------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `sfp` | Unsure; `show sfp`? Maybe covered by port? |
| `sfp <sfp_number> [10g\|2g5\|1g\|100m\|auto]` | Unsure; this is the only place that “sfp_number” exists. Move to port? |
| `port 1 show` | `show port 1` |
| `port 1 name <name>` | `port 1 name <name>` |
| `port 1 lldp [block\|permit]` | `port 1 lldp [block\|permit]` |
| `port 1 cabletest [show\|run]` | `debug cabletest run port 1 cabletest; show debug cabletest port 1` |
| `port 1 10m [half\|full]` | `port 1 speed [10\|100m] [half\|full]` |
| `port 1 100m [half\|full]` | `port 1 speed [10\|100m] [half\|full]` |
| `port 1 10g` | `port 1 speed 10g` |
| `port 1 5g` | `port 1 speed 10g` |
| `port 1 2g5` | `port 1 speed 10g` |
| `port 1 1g` | `port 1 speed 10g` |
| `port 1 auto` | `port 1 speed 10g` |
| `port 1 off` | `port 1 down` |
| `port 1 on` | `port 1 up` |
| `port 1 duplex [full\|_else_]` | ?? Why have this separate from the speed? |
| `mtu show` | I think should be on `show port` |
| `mtu 1 1500` | `port 1 mtu 1500` |
| `syslog` | `show system syslog` |
| `syslog [on\|off]` | `system syslog on\|off` |
| `syslog ip <0.0.0.0>` | `system syslog server 1.2.3.4` |
| `syslog port <514>` | `system syslog port 514` |
| `dns server <0.0.0.0>` | `system dns server 1.1.1.` |
| `dns lookup <record>` | `debug dns lookup 1.1.1.1` |
| `time` | `show system time` |
| `ntp` | `show system ntp` |
| `ntp on` | `system ntp on` |
| `ntp off` | `system ntp off` |
| `ntp server` | `system ntp server 10.2.3.4` |
| `ntp interval <600>` | `system ntp interval 600` |
| `ntp timezone +02:30` | `system ntp timezone ` |
| `ntp dst [off\|eu\|us]` | `system dst …` |
| `ip dhcp` | `system ip dhcp on` |
| `ip 123.45.67.89` | `system ip static 123.45.67.89/24` |
| `gw` | `show system ip gateway` |
| `gw 123.45.67.89` | `system ip gateway 123.45.67.89` |
| `netmask` | --removed and moved into ip command |
| `netmask 123.45.67.89` | --removed and moved into ip command |
| `l2` | `show system mac-table` |
| `l2 forget` | `system clear-mac-table` |
| `igmp on` | `system igmp on` |
| `igmp off` | `system igmp off` |
| `igmp show` | `show system igmp` |
| `mac` | `show system mac` |
| `mac <aa:bb:cc:dd:ee:ff>` | `system mac <aa:bb...>` |
| `hostname` | `show system hostname` |
| `hostname <newnamehere>` | `system hostname` |
| `stp [on\|off\|clear]` | `feature stp [on\|off\|clear]` |
| `stp [on\|off\|clear]` | `show feature stp` |
| `stp [port\|lag] <pnum\|lagnum> [on\|off\|mcheck]` | `port 1 stp [on\|off\|mcheck]` |
| `stp [port\|lag] <pnum\|lagnum> edge [on\|auto\|off]` | `port 1 stp-edge [on\|auto\|off]` |
| `stp [port\|lag] <pnum\|lagnum> cost 200` | `port 1 stp-cost xxx` |
| `stp [port\|lag] <pnum\|lagnum> p2p [auto\|on\|off]` | `port 1 stp-p2p [on\|auto\|off]` |
| `stp [port\|lag] <pnum\|lagnum> prio 123456` | `port 1 stp-priority 1234` |
| `stp [port\|lag] <pnum\|lagnum> guard [bpdu\|root\|none]` | `port 1 stp-guard [bpdu\|root\|none]` |
| `stp [port\|lag] <pnum\|lagnum> filter [on\|off]` | `port 1 stp-filter [on\|off]` |
| `stp version [rstp\|stp]` | `stp version [rstp\|stp]` |
| `stp pathcost [long\|short]` | `stp pathcost [long\|short]` |
| `stp bpdu [filter\|flood]` | `stp bpdu [filter\|flood]` |
| `stp prio 15` | `stp priority 15` |
| `stp hello 10` | `stp hello 10` |
| `stp maxage 30` | `stp maxage 30` |
| `stp fwd 4` | `stp fwd 4` |
| `stp txhold 5` | `stp txhold 5` |
| `lacp show` | `show lag lacp` |
| `lacp off` | Not sure, this just disables all LACP; when would you do this? |
| `lacp off 1` | `remove lag 2 lacp or `lag 2 static 1 2 3 4` |
| `pvid <port> <vid>` | `port 1 pvid 3` |
| `vlan 2 show` | `show vlan 2` |
| `vlan 2 mgmt` | `system mgmt-vlan 2` |
| `vlan 2 d` | `remove  vlan 2` |
| `vlan 2 <name>` | `vlan 2 <name>` |
| `vlan 2 <name> 1 2t 3` | `port 1 untagged 1; port 2 tagged 2; port 3 untagged 3` |
| `isolate [show]` | `show port-isolation` |
| `isolate [off]` | `remove port-isolation` |
| `isolate <port> <list of other port that the port can access>` | `port 2 limit-to <list of ports>` |
| `mirror [status]` | `show mirror` |
| `mirror <monitor-port> <list of ports to mirror>` | `port 2 mirror-from <list of ports to mirror from>` |
| `lag show` | `show lag` |
| `lag <number> d` | `remove lag 2` |
| `lag <number> [lacp] <list of ports>` | `lag 2 [static\|lacp] 1 2 3 4` |
| `laghash <number> <list of hashes>` | `lag 2 hash <list of hashes>` |
| `regget <hex value>` | `debug regget <hex value>` |
| `regset <register hex> <value hex>` | `debug regset <register hex> <value hex>` |
| `sdsget <id> <page> <reg>` | `debug sdsget <id> <page> <reg>` |
| `sdsset <id> <page> <reg> <val>` | `debug sdsset <id> <page> <reg> <val>` |
| `phyget <id> <devid> <ereg>` | `debug phyget <id> <devid> <ereg>` |
| `physet <id> <devid> <reg> <val>` | `debug physet <id> <devid> <reg> <val>` |
| `rnd` | `debug rnd` |
| `passwd <new password>` | `system passwd <pass>` |
| `eee [on\|off] <port>` | `port <port> eee [on\|off] OR remove port 1 eee` |
| `eee status <port>` | `show eee <port>` |
| `eee on <port> [100m\|1g\|2g5]` | `port <port> eee <speed>` |
| `bw [status] <port>` | `show bw <port>` |
| `bw in 1 <hex value>` | `port 1 bw-in-max <hex>` |
| `bw in 1 [drop\|fc]` | `port 1 bw-in-action [drop|fc|off]` or maybe add onto above |
| `bw in 1 off` | `remove port 1 bw-in` |
| `bw out off` | `port 1 bw-out off` |
| `bw out <hex value>` | `remove port 1 bw-out` |
| `storm <port> [bcast\|mcast\|ucast\|umcast] <value> [pps\|kbps]` | `port 1 storm [bcast\|mcast…] <val> [pps\|kbps]` |
| `storm <port> [bcast\|mcast\|ucast\|umcast] off` | `remove port 1 storm [bcast\|mcast] …` |
| `ingress [uta]` | ?? not sure; this applies to all ports; remove it? |
| `ingress <list of ports and modes>` | `Port 1 ingress-filter [uta]` |
| `session <number>` | `system session-timeout <number>` |
| `lldp [on\|off]` | `feature lldp on` |

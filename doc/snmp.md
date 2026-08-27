# SNMP

This firmware ships a small read-only SNMPv1 / SNMPv2c agent on
UDP port 161. Its purpose is to expose the state that already lives
on the switch (system identity, uptime, per-port link state, per-port
counters) to any off-the-shelf monitoring tool - Grafana with
`snmp_exporter`, LibreNMS, Zabbix, PRTG, `net-snmp` from the shell,
etc. There is no support for SNMPv3, no support for `SetRequest`, and
no traps.

## Enabling the agent

The agent is compiled in but disabled by default. From the serial
console:

```
snmp on
```

To make the change persistent, add the same line to the running
configuration (via the web UI's *System* page or by editing
`config.txt` before flashing).

Configuration commands:

| command | effect |
|---------|--------|
| `snmp on` | start the agent |
| `snmp off` | stop the agent |
| `snmp community <string>` | change the read community (default `public`) |
| `snmp contact <string>` | set `sysContact.0` (default empty) |
| `snmp location <string>` | set `sysLocation.0` (default empty) |
| `snmp` (no argument) | print current status |

`sysName.0` is served from the switch's `hostname` variable and
therefore follows the `hostname` command.

## Testing from the LAN

```
snmpwalk -v2c -c public 192.168.0.5 system
snmpwalk -v2c -c public 192.168.0.5 ifTable
```

Or, to browse everything the agent serves:

```
snmpwalk -v2c -c public 192.168.0.5 .1.3.6.1.2.1
```

## Served MIB

### `system` group (`.1.3.6.1.2.1.1`)

| OID | value | source |
|-----|-------|--------|
| `sysDescr.0` | `RTLPlayground <ver> on <machine>` | build info |
| `sysObjectID.0` | `.1.3.6.1.3.99.99.1` (unregistered) | fixed |
| `sysUpTime.0` | TimeTicks since boot | `ticks / 2` |
| `sysContact.0` | `snmp contact` value | config |
| `sysName.0` | `hostname` | config |
| `sysLocation.0` | `snmp location` value | config |
| `sysServices.0` | `3` (physical + datalink) | fixed |

### `interfaces` group (`.1.3.6.1.2.1.2`)

`ifIndex` runs from 1 up to the number of ports on the device (as
reported by `ifNumber.0`). Physical port labels are mapped to
`ifIndex` in the same order the CLI's `stat` command lists them.

Served ifTable columns:

| column | OID suffix | type | value |
|--------|------------|------|-------|
| `ifIndex` | `.1.1.<n>` | INTEGER | `n` |
| `ifDescr` | `.1.2.<n>` | OCTET STRING | `port <label>` |
| `ifType` | `.1.3.<n>` | INTEGER | `6` (ethernetCsmacd) |
| `ifMtu` | `.1.4.<n>` | INTEGER | `1500` |
| `ifSpeed` | `.1.5.<n>` | Gauge32 | link speed in bits/s (capped at 2^32-1 for 5 G / 10 G links) |
| `ifPhysAddress` | `.1.6.<n>` | OCTET STRING | CPU MAC address |
| `ifAdminStatus` | `.1.7.<n>` | INTEGER | always `1` (up) |
| `ifOperStatus` | `.1.8.<n>` | INTEGER | `1` (up) if link is up, `2` (down) otherwise |
| `ifInOctets` | `.1.10.<n>` | Counter32 | RX packet count (byte counters are not exposed by the ASIC) |
| `ifInUcastPkts` | `.1.11.<n>` | Counter32 | RX packet count |
| `ifInErrors` | `.1.14.<n>` | Counter32 | error packet count |
| `ifOutOctets` | `.1.16.<n>` | Counter32 | TX packet count (byte counters are not exposed by the ASIC) |
| `ifOutUcastPkts` | `.1.17.<n>` | Counter32 | TX packet count |
| `ifOutErrors` | `.1.20.<n>` | Counter32 | error packet count |

### Limitations

* Only `GetRequest`, `GetNextRequest` and `GetBulkRequest` are
  accepted. `GetBulk` is answered with one variable-binding per
  request-varbind - non-repeaters and max-repetitions are ignored.
  Tools such as `snmpbulkwalk` still work but do more round trips
  than they would against a full-featured agent.
* Received packets are matched against a single, static community
  string; anyone with the community can walk the MIB. Do not enable
  the agent on interfaces where this is not acceptable.
* No IPv6, no SNMPv3, no ifXTable (64-bit counters), no bridge MIB,
  no LLDP MIB. Only the columns above.
* Byte counters are not exposed by the ASIC to the CPU port
  directly; `ifInOctets` / `ifOutOctets` report the RX / TX packet
  count so that graphs still show growth over time.

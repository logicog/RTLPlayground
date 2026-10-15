# Read-only SNMPv2c

Build with `make MACHINE=<profile> SNMP=1`. Without SNMP=1 the agent is omitted.
Generic UDP validation, NIC RX bounds, endian handling and startup CRLF support
are unconditional. The generic UDP allocator stays in BANK1 in both configurations.

The agent starts disabled and unconfigured. There is no default community or
compile-time credential header. Use the authenticated console or UART:

```
snmp community <1-to-32-printable-ASCII-characters>
snmp manager 192.0.2.250/32
snmp on
snmp show
snmp off
```

Use your actual trusted manager address; the example is an RFC 5737 alias.
Manager prefixes 1–32 are accepted; host bits are normalized to the network.
Invalid input preserves current settings. Changes apply immediately but require
the existing **Save to Flash** confirmation to survive reboot. Save serializes
manager, community and on/off from runtime state, not stale command history.
No new Flash regions are used. Existing startup records restore the same state;
old `snmp changed` metadata is ignored during replay, rejected from live CLI,
and omitted from new saves. Configuration-change timestamps are not implemented.

The System page adds a status/on-off card to the normal upstream layout.
Community values are masked in the configuration editor and Web Console, never
included in `snmp show` or `/information.json`. Startup Flash, UART history and
authenticated configuration HTTP responses still contain plaintext; browser
masking is not secret isolation. SNMPv2c and HTTP are unencrypted. Restrict
management access and set a narrow source ACL. A subnet ACL excludes network and
broadcast source addresses, except /31 and /32 point-to-point/host cases.

## Objects

GET and GETNEXT expose the following read-only objects:

- system: sysDescr.0, sysUpTime.0, sysContact.0 (empty), sysName.0 and
  sysLocation.0 (empty). sysUpTime is Counter32-width TimeTicks in centiseconds.
- ifNumber.0; ifTable columns ifIndex, ifDescr, ifType, ifSpeed, ifOperStatus,
  ifInOctets, ifInUcastPkts, ifOutOctets and ifOutUcastPkts.
- ifXTable: ifName, ifHCInOctets, ifHCInUcastPkts, ifHCInMulticastPkts,
  ifHCInBroadcastPkts, ifHCOutOctets, ifHCOutUcastPkts,
  ifHCOutMulticastPkts, ifHCOutBroadcastPkts and ifHighSpeed.

Octet/unicast ifTable counters use Counter32; native high-capacity counters use
Counter64. Speeds are Gauge32 (ifSpeed in bit/s, saturating at UINT32_MAX;
ifHighSpeed in Mbit/s). A down/unknown-speed port retains both speed rows with
zero values. Hardware read failures return genErr rather than fabricated zeros.
Indices identify actual front-panel ports, including SFP, without a CPU row.
Admin state, MTU and per-port MAC are not measured or synthesized.

sysObjectID remains unsupported pending an authoritative RTLPlayground PEN/OID
from the upstream maintainer. No vendor's enterprise namespace is borrowed.
SET, GETBULK, SNMPv3, notifications and writable system strings are unsupported.
Requests are limited to 484 bytes, eight varbinds and 32 OID arcs.

## Verification and limits

`make -C test` runs host subsystem, SNMP, transport, hardware-adapter, CLI and
startup replay tests with sanitizers. Use `CC=clang` for the other compiler.
Local JavaScript tests use `make -C test snmp-gui`; the optional browser test is
`node test/test_system_layout.cjs` with an available Playwright installation.
Firmware checks use SDCC 4.5.0, with per-bank map/memory inspection on both chip
families. Host register/Flash mocks do not establish hardware timing or stack
high-water. The bounded statistics adapter retains timeout/quarantine behavior;
STAT_GET's unbounded waits are not substituted into a network request path.
Runtime stack high-water remains unmeasured. Review image target/CRC, backups and
recovery separately before any hardware upgrade.

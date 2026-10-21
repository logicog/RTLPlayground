# LLDP

The switch sends an IEEE 802.1AB LLDPDU on every permitted port every 30 s
(TTL 120 s) and learns one neighbour per port from the LLDPDUs it receives.
Both are shown on the LLDP page of the web UI and by the `lldp` command.

## Commands

| Command | Effect |
|---|---|
| `lldp` | Show the state, the optional TLVs and the neighbour of every port |
| `lldp on` / `lldp off` | Send LLDPDUs and trap received ones (01:80:c2:00:00:0e) to the CPU, or stop both. Off by default |
| `lldp tlv <portdesc\|syscap\|mgmt\|pvid\|macphy\|mfs> on\|off` | Switch an optional TLV on or off for all ports. All are on by default |
| `port <n> lldp permit\|block` | Send LLDPDUs on port n, or not. All ports are permitted by default |

All of them can be saved in the startup configuration.

## What is sent

Every LLDPDU carries the mandatory TLVs, the system name (when a hostname is
set) and the system description, then the optional TLVs that are switched on,
then End. TLV header: 7 bit type, 9 bit length.

| TLV | Type | Value bytes |
|---|---|---|
| Chassis ID | 1 | `04` (MAC address) + 6 byte switch MAC |
| Port ID | 2 | `07` (locally assigned) + the port number as one ASCII digit |
| TTL | 3 | `00 78` (120 s) |
| Port description (`portdesc`) | 4 | the port name; not sent when the port has no name |
| System name | 5 | the hostname |
| System description | 6 | the machine model, e.g. `keepLink KP-9000-9XHML-X V3.2` |
| System capabilities (`syscap`) | 7 | `00 04 00 04`: Bridge supported, Bridge enabled |
| Management address (`mgmt`) | 8 | `05 01` + IPv4 address + `01 00 00 00 00` (interface numbering unknown) + `00` (no OID); not sent without an IP address |
| Port VLAN ID (`pvid`) | 127 | `00 80 c2 01` + PVID (2 bytes) |
| MAC/PHY configuration/status (`macphy`) | 127 | `00 12 0f 01` + autonegotiation (bit 0 supported, bit 1 enabled; 0 on SFP ports) + advertised 10/100/1000 abilities (2 bytes, ifMauAutoNegCapAdvertisedBits) + operational MAU type (2 bytes, 0 with the link down) |
| Maximum frame size (`mfs`) | 127 | `00 12 0f 04` + the port's maximum frame length (2 bytes, as set by `mtu`) |
| End | 0 | none |

The MAU type follows the link speed: 10BASE-T 10/11, 100BASE-TX 15/16,
1000BASE-T 29/30, 2.5GBASE-T 103, 5GBASE-T 104, 10GBASE-T 54 on copper ports
(half/full duplex); 100BASE-FX 18, 1000BASE-X 22, 2.5GBASE-X 110, 5GBASE-R 112,
10GBASE-R 33 on SFP ports.

## Neighbours

The RTL tag of a trapped frame names the port it came in on. The newest LLDPDU
of a port replaces the stored neighbour, which keeps:

- chassis ID and port ID with their subtypes; a MAC address subtype, or an ID
  with bytes that are not printable, is shown as hex bytes (up to 10)
- port description, system name (up to 31 characters each), system description
  (up to 63 characters)
- supported and enabled system capabilities
- the first IPv4 management address
- the 802.1 Port VLAN ID
- the TTL and the age, the seconds since the LLDPDU arrived

Received frames are untrusted. A frame is dropped, and the stored neighbour
kept, when a TLV runs past the end of the frame, when the first three TLVs are
not Chassis ID, Port ID and TTL (each at least 2 bytes long), or when one of
these comes again later. Optional TLVs that are too short to hold their value
are ignored. Strings are cut to their size, and bytes outside printable ASCII
as well as `"` and `\` are stored as `.`, so they go out unchanged in JSON and on
the console.

A neighbour is removed when its TTL runs out, when a shutdown LLDPDU (TTL 0)
arrives, and when the link of its port goes down.

## /lldp.json

`/lldp.json` holds the settings and a neighbour summary of every port, at most
1771 bytes for 9 ports with every field at its longest (`test/test_lldp.c`):

```json
{"on":1,"tlv":63,"ports":[
 {"p":1,"tx":1,"nb":{"ch":"00:1b:54:aa:10:01","pi":"Gi1/0/24","sn":"core-sw1","ip":"10.0.0.1","cap":4,"age":12}},
 {"p":2,"tx":0}]}
```

`tlv` has the bits of the optional TLVs in the order of the `lldp tlv` names
(portdesc = 1 ... mfs = 32), `tx` is the per-port send setting, `nb` is missing
without a neighbour, `ip` is empty without a management address and `cap` holds
the enabled capabilities.

`/lldp.json?port=<n>` returns one port with what only the pop-ups need: the PVID
and maximum frame size it sends, and the neighbour's subtypes (`cs`, `ps`),
port and system description (`pd`, `sd`), supported capabilities (`sc`),
Port VLAN ID (`vid`, 0 when not sent) and TTL.

## Limits

- One neighbour per port: with several LLDP agents behind one port (an
  unmanaged switch in between), the neighbour flips between them.
- Only the TLVs listed above are decoded; others are skipped.
- No LLDP-MED, no shutdown LLDPDU on `lldp off`, fixed 30 s interval and 120 s TTL.

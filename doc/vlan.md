# VLAN

The RTL827x provides support for up to 4096 802.1Q VLANs, each port can be
assigend a PVID.

## VLAN control
VLANs are controlled by the VLAN table. Configuration of entries is done as
for the L2 entries, apart from that the ASIC does not add entries by itself.

Adding a VLAN entry is done by setting:

```
RTL837x_TBL_DATA_IN_A = 02 0v vv vv
```
where 0x02 designates a valid entry and vvvvv is a 20bit field made of lower 10bits designating whether a
port is a member of a VLAN. The higher 10 bits are '0' for tagged ports
and '1' for untagged ports (the bit-logic definition is `v = (~members) ^ tagged ^
members`). Ports are numbered 0-9, 9 being the CPU-port. Note that for
the RTL8372 devices, the ports are not in their physical order.

Once RTL837x_TBL_DATA_IN_A is set, the entry is added to the table by:

```
RTL837X_TBL_CTRL = 0V VV TT CC
CC: TBL_WRITE | TBL_EXECUTE
VVV: VLAN-Id
TT: 0x02 (TBL_VLAN)
CC: 0x03 (TBL_WRITE | TBL_EXECUTE)
```
The ASIC will clear bit 0 once the entry has been added.

An entry is deleted by adding an invalid entry (00 instead of 0x02 in
RTL837x_TBL_DATA_IN_A).

A port is assigned a PVID by setting the PVID-bits of the corresponding
register of the port. 2 ports share a register. An odd port uses bits [23:12],
an even port uses bits [11:0].  The base register is
RTL837x_PVID_BASE_REG (0x4e1c) and the registers go to 0x4e2c so that also
the CPU-Port may have a PVID.

Register RTL837x_REG_INGRESS (0x4e10) allows to define the ingress rules of
a port. 2 bits define a rule and bits 0-19 are being used. A value of 00
defines no filtering, 01 (0x01) allows only tagged packets, while 10 (0x02)
allows only untagged packets to enter a port.

Register RTL837X_VLAN_PORT_IGR_FLTR (0x4e18) enables or disables ingres VLAN
filtering, each bit corresponds to given port (port0 -> bit0, port9 -> bit9).
When enabled, incomming package's vlan tag is checked against VLAN membership
on given port. When package contains VLAN not in member list, package is dropped.

The default PVID on all port is 1, ingress VLAN filtering is enabled and all types of
frames are accepted on input on all ports.

On the RTL836xB the PVID register (0x0700-0x0705) holds an *index* into the
member-config table, not a VLAN ID, and index 0 is empty on this board. Every
port that would otherwise come up on that empty index is therefore given the
default PVID 1 at boot: `port_pvid_default_unset()` runs right after the saved
configuration is replayed, so a real `pvid` line wins and only genuinely unset
ports move. The CPU port (fabric 10, register 0x0705) is included, and its row
is shown by `vlan show` (see below). It runs before any filter is armed, so it
changes what a port reads as its PVID without taking the board off the network.

On the RTL836xB the 4K (CVLAN) entry is **three** words, and the member and
untag fields are **eleven** bits wide (fabric ports 0-10), split across two of
them: member ports 0-7 and untag ports 0-7 live in word 0, member ports 10:8 in
word 2 bits 2:0 and untag ports 10:8 in word 2 bits 5:3 (bit 6 is meteridx
bit 5). That is the packing in the RTL8367C SDK's `_rtl8367c_Vlan4kStUser2Smi()`
- the SDK whose HAL describes this exact die (`rtl8370b_hal_Ctrl`: maximum
physical port 10, CPU = EXT_PORT2). The CPU/ring NIC is fabric port 10, so its
member bit is word 2 bit 2. This port used to write only words 0 and 1, so the
CPU could never be a member of a VLAN; `rtl836x_vlan4k_set()` writes word 2 now,
and every VLAN names the CPU - untagged in the default VLAN 1, tagged in a
user-created VLAN (`vlan_create()`), matching the vendor SDK
(`mbrmsk | BIT(CPU_PORT)`). Without that member bit, turning the global 0x07a8
filter on starves the CPU and the board loses ping and web.

The default VLAN 1 is ensured on every boot even when every port already has a
PVID: `port_pvid_default_unset()` reads the 4K entry, creates it if missing and
adds the CPU member bit if an older image programmed it without one. That is
what keeps the filter from cutting management.

## Default filtering state (RTL836xB)
Ingress VLAN filtering is **on by default**, matching the RTL8372/3 machines.
At boot the RTL836x path runs `port_pvid_default_unset()` and then
`port_vlan_filter_default_on()` (rtlplayground.c), which writes 0x07a9 = 0xff
(one bit per UTP port) and sets the global 0x07a8 gate. The other machines'
`vlan_setup()` does the same unconditionally at boot; only the order is
different here, because the PVID default must land first so no port is left on
an empty MC entry.

This is safe only because the CPU/ring NIC is a member of the default VLAN:
with the 4K member field widened to eleven bits (word 2) and the CPU written
into it, VLAN 1 reads back `member=0x04ff untag=0x04ff` and the gate no longer
starves management. A saved configuration still wins - it is replayed before
the default - so a `pvid`/`vlan` layout defines what the filter enforces. There
is no per-port console command to change it: to clear the filter wholesale, use
`regset 07a8 0000` (global gate) and `regset 07a9 0000` (all port bits).

## Management VLAN and TX tagging (RTL836xB)
The RTL836x now tags its own management frames with `management_vlan` before
handing them to the ring NIC, exactly as the RTL8372/3 path always did. That is
what makes `vlan <id> mgmt` work on this die: the CPU port's PVID is fixed at
VLAN 1 (there is no console command to change it), so without the tag a reply
was always classified into VLAN 1 and could only leave through a VLAN 1 port.
With the tag the frame follows the management VLAN, and `vlan <id> mgmt`
selects which ports can reach the switch.

Three pieces make it work together:
- the CPU is a member of every VLAN (the 4K word-2 member bit), so the frame is
  not filtered on its way to the CPU;
- `port_vlan_egress_original()` sets each front port's egress mode to ORIGINAL
  (`0x000e + p*0x20`, bits 5:4 = `0b00`) at boot. The chip's reset default is
  REAL KEEP (`0b11`, measured live as `0x48b0` on every port), which transmits
  an inserted tag verbatim; ORIGINAL "output will follow VLAN untag setting",
  so the tag is stripped again for an untagged member and the host still sees a
  plain untagged frame;
- `tcpip_output()` inserts the 802.1Q header on this machine too. It used to
  be compiled out with a "no 802.1Q insertion" note, which was correct only
  while VLAN processing was off (`0x07a8 = 0`) and egress stayed REAL KEEP.

Verified live on the SL-SG008W: with the host on port 6 moved off VLAN 1 into
VLAN 100 and `vlan 100 mgmt`, ping and the authenticated web UI both work from
port 6. Setting `management_vlan` back to 1 moves management back to the VLAN 1
ports (port 8 works, port 6 stops). Setting one port's egress mode back to REAL
KEEP breaks that port again - the A/B proof that the egress mode is
load-bearing. Untagged traffic on a flat VLAN 1 board is unaffected.

By default, the ports transmit Ethernet frames with Realtek's proprietary
tag format. By setting bit 6 (0x40) of the respective port configuration
registers 0x1238, 0x1338, ...

## VLAN API
The code currently provides the following functions:
```
void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked;
uint16_t port_pvid_get(uint8_t port) __banked;
void vlan_create(void) __banked;   // reads from global vlan_settings
int8_t vlan_get(register uint16_t vlan) __banked;  // returns data in sfr_data
void vlan_delete(uint16_t vlan) __banked;

```

# VLAN configuration on the Serial Console
For testing the following commands are provided on the serial console:
```
vlan <VLAN-ID> p[t]...
  create or set vlan with given ID and the list of ports as members, a `t`
  behind a port defines the port as a tagged member.

vlan <VLAN-ID> d
  deletes the VLAN

vlan show
  Dumps the current ingress vlan settings. On the RTL836xB the dump ends with
a `CPU` row for fabric port 10 (its PVID from register 0x0705, its acceptable
frame type from 0x07ab, Filtering always `Disabled` -- 0x07a9 has no CPU bit).
This is what makes the boot-time default PVID 1 on the CPU port visible without
a raw `regget 0705`. The CPU port is read-only here: `pvid` and `ingress` still
refuse it.

vlan <VLAN-ID> mgmt
  Restricts network access to the switch (web UI, syslog) to the given
  VLAN. Use `vlan 0 mgmt` to disable the filter. Default is `vlan 1 mgmt`.
  On the RTL836xB the switch tags its management frames with this VLAN, so a
  port can only reach the switch if it is a member of it: `vlan 100 mgmt`
  serves the ports in VLAN 100 (and stops serving the VLAN 1 ports).
  Warning: setting this to a VLAN no reachable port is a member of locks out
  the web UI; recovery requires the serial console.

pvid <port> <VLAN-ID>
  assigns PVID to a port. ports are numbered as on the casing

ingress [p]<t|u|a>...
  Allows ingress packages on port `p` only when `t`agged, `u`ntagged or `a`ny.
  Multiple ports can be given at once as in vlan. When `p` is missing, all ports
  are assigned the same mode. CPU port can not be changed.

  Use `vlan show` to see current configuration.

  Example:
  `ingress 1t 2a` -> Set port 1 as tagged input only, set port 2 accepting any frames.
  `ingress a` -> Set all ports to accept both tagged and untagged frames (default behaviour).

```

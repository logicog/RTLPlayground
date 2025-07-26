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
register of the port. 2 ports share a register. One port uses the higher
16 bits, the other (even ports) use the lower. The base register is
RTL837x_PVID_BASE_REG (0x4e1c) and the registers go to 0x4e2c so that also
the CPU-Port may have a PVID.

Register RTL837x_REG_INGRESS (0x4e10) allows to define the iingress rules of
a port. 2 bits define a rule and bits 0-19 are being used. A value of 00
defines no filtering, 01 (0x01) allows only tagged packets, while 10 (0x02)
allows only untagged packets to enter a port. The default PVID is 1.

By default, the ports transmit Ethernet frames with Realtek's proprietary
tag format. By setting bit 6 (0x40) of the respective port configuration
registers 0x1238, 0x1338, ...

## VLAN API
The code currently provides the following functions:
```
void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked;
void vlan_create(uint16_t vlan, uint16_t members, uint16_t tagged) __banked;
void vlan_delete(uint16_t vlan) __banked;

```

# VLAN configuration on the Serial Console
For testing the following commands are provided on the serial console:
```
vlan <VLAN-ID> p[t/u]...
  create or set vlan with given ID and the list of ports as members, a t
  behind a port defines the port as a tagged member, the u is optional

vlan <VLAN-ID> d
  deletes the VLAN

pvid <port> <VLAN-ID>
  assigns PVID to a port. ports are numbered as on the casing

ingress <port> [tagged|untagged|all]
  Allows ingress only for the named packages at the given port
```

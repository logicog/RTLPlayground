# MACsec (IEEE 802.1AE)
MACsec encrypts and authenticates every frame on a link between two ports.
The RTL8373 has a MACsec engine in front of each of its internal 2.5G PHYs,
the ports the switch counts as 4 to 7, and RTLPlayground drives it with a
static key: both ends are given the same 128-bit key by hand, there is no
key agreement protocol (MKA) and no rekeying.

## Commands
```
macsec <port> key <32 hex digits> peer <16 hex digits>
macsec <port> off
macsec
```
`key` is the 128-bit secure association key (SAK) shared with the peer.
`peer` is the secure channel identifier (SCI) the peer puts into its frames:
its MAC address followed by its 16-bit port identifier. `macsec` without
arguments lists the protected ports with the SCI the switch uses on each one,
the next packet number of the transmit association and the counters of the
engine: frames encrypted, frames received and verified, frames that failed
verification, frames without a MACsec tag and frames with an unknown SCI.
The settings are kept in the configuration like any other command, which puts
the key into the flash in clear text.

With MACsec on, every frame leaving the port is encrypted with an explicit
SCI (GCM-AES-128, confidentiality offset 0) and every frame arriving is
accepted only if it carries a MACsec tag with the peer's SCI, decrypts, and
has a packet number above the last one seen. Frames without a tag, with a
bad tag or from another SCI are dropped in both directions. The switch's own
frames on that port, STP, LLDP and LACP included, go through the engine as
well, so the peer sees them on its MACsec interface.

A Linux host on the other end of the cable is set up with the same key:
```
ip link add link eth0 macsec0 type macsec encrypt on
ip macsec add macsec0 tx sa 0 pn 1 on key 01 <32 hex digits>
ip macsec add macsec0 rx sci <switch SCI>
ip macsec add macsec0 rx sci <switch SCI> sa 0 pn 1 on key 02 <32 hex digits>
ip link set macsec0 up
```
`<switch SCI>` is the one `macsec` prints for the port; the host's own SCI,
shown by `ip macsec show`, is the `peer` argument on the switch.

The packet number is 32 bits wide and the key is never changed, so a link
can carry at most 2^32 frames in each direction before the association has
to be set up again with a new key, about six hours of 1500-byte frames at
2.5 Gbit/s. The ports behind the RTL8224 PHY (switch ports 0 to 3 on an
RTL8373) have an engine of their own that is not supported yet.

## Registers
Each port has a wrapper of 72 registers in the switch register space,
starting at `0xc0f0 + 0x800 * (port - 4)`. `RTL837X_MACSEC_GLB_SET1` holds
the bypass bits for both directions (1, 2, 9, 10), the software reset
enables (0, 8) and the gating bits (30, 31) that have to be set, and
`RTL837X_MACSEC_GLB_MASK` bits 0, 4, 8 and 12 polled, around any change of
the bypass bits. After reset both directions are bypassed and
`RTL837X_MACSEC_PM_CTRL` gates the clock of the engine off; bits 0 and 1
turn it on.

The engine itself is an EIP-160 with one instance per direction, reached
through an indirect window. The egress instance takes its address in bits
16 to 31 of `RTL837X_MACSEC_ADDR_AE`, its data in `RTL837X_MACSEC_RWDH_AE`
with the low half in bits 16 to 31 and the high half in bits 0 to 15, and
the request bits in `RTL837X_MACSEC_CMD_AE` (0 write, 4 read). The ingress
instance takes the address in bits 0 to 15 of `RTL837X_MACSEC_ADDR_AI` with
the request bits 16 (write) and 20 (read), the low data half in
`RTL837X_MACSEC_RWDL_AI` and the high half in bits 16 to 31 of
`RTL837X_MACSEC_CMD_AE`. The read request bits clear themselves.

Inside the engine the register layout is the one the Linux kernel programs
in `drivers/net/phy/mscc/mscc_macsec.c`, with the word addresses of that
driver multiplied by four. Both instances report version `0x04325fa0` at
`0xfffc`. Transform records of 20 words start at 0 (control word, context
id, key, hash key, packet number, then the SCI for egress or the replay
window and the SCI for ingress), the match rules of the security
associations at `0x4000` with 0x40 per entry, their flow controls at
`0x7000`, the enable and disable bitmaps at `0x6080` and `0x60c0`, the
actions for frames matching no rule at `0x7944` and `0x7948`, and the
counters at `0x8000` with 0x80 per association. The hash key is the AES
encryption of a zero block with the SAK, computed by the firmware.

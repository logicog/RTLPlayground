# The CPU Port

The RTL827x provide a CPU Port for a NIC on the 8051 side of the SoC.

## Receiving packets
Packets are received by either polling the RTL837X_REG_RX_AVAIL register
(0x7874), which will be > 0 if data is within a ring-buffer on the ASIC side
of the SoC. Alternatively, an interrupt can be triggered (EX1).

Data is transferred to the 8051 side by calling an SFR function. First, the
frame header of the received frame will be copied over. For this, provide
the destination memory location in xdata memory in SFRs B3 and B4 (little
endian), the source location on the ASIC-side in SFRs B5/B6 (also little
endian, found in RTL837X_REG_RX_RINGPTR, 0x787c) and execute the function
by setting SFR_NIC_CTRL (B7) to the length to be transferred divided by 8,
i.e. 1.

The frame header has the following format:
```
SS xx xx xP LL LH xx xx
SS:	8-bit sequence number
P:	Port number
LHLL:	Length of Ethernet frame (little endian)
xx:	Unknown
``` 

Next, transfer the actual packet over by repeating the SFR function with a
pointer to the frame on the ASIC directly after the frame header and a
length as given by the length in the frame header + 7, again divided by 8.

The received frame will have an RTL proprietary Ethernet frame type of
0x8899 (RRPC) where normally the frame type 0x0800 for IPv4 would be located.
Further 6 bytes follow describing the frame, before the normal IPv4 data
starts. A documentation can be found here:
[TAG8899_COMMIT](https://github.com/torvalds/linux/commit/1521d5adfc2b557e15f97283c8b7ad688c3ebc40)

After copying over header and frame, the frame is marked read in the ring
buffer on the ASIC side by writing 0x1 to RTL837X_REG_RX_DONE (0x784c).

## Transmissing packets
Packets are transmitted by preparing a frame-header plus frame in xdata memory
and transferring both to the ASIC side via the SFRs.

```
SS 07 00 00 LL LH 00 00 
SS:	8-bit sequence number
07:	Enables header and TCP checksum offloading to ASIC
LHLL:	Length of the Ethernet frame
```
The Ethernet frame data starts immediately after the frame header in xdata
memory. The frame is transferred to the ASIC side by setting SFRs B3 and B4
to the xdata source address of the frame header, and the ring pointer to the
free space indicated by register 0x7890 multiplied by 8 and the MSB set.
The length is given by the length of the frame plus 15, divided by 8.

Writing 0x1 to register 0x7850 will transmit the frame. The Ethernet frame
checksum and the TCP checksum are automatically calculated (offloaded) by the
ASIC before transmitting on the wire.


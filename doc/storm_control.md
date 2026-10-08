# Storm Control
Storm control limits the rate at which a port admits broadcast, multicast,
unknown unicast and unknown multicast frames. Frames of a type above its limit
are dropped at ingress, so a loop or a misbehaving host cannot flood the rest
of the network through that port. Known unicast traffic is not affected.

## Commands
```
storm <port> bcast|mcast|ucast|umcast <rate> pps
storm <port> bcast|mcast|ucast|umcast <rate> kbps
storm <port> bcast|mcast|ucast|umcast off
storm
```
`ucast` is unicast to a MAC address the switch has not learned yet, and
`umcast` is multicast without an entry in the address table. Multicast groups
get their entries from IGMP snooping, so without `igmp on` all multicast is
unknown and only the `umcast` limit applies to it; `mcast` covers the groups
IGMP has learned. A limit in
packets per second goes from 1 to 1048575, a limit in kbit/s from 1 to
10000000. `storm` without arguments prints the limits of every port. The
limits are kept in the configuration like any other setting, and the Bandwidth
page of the web interface has a Storm control card to set them.

## Registers
Each port and type has an enable bit and a meter index, and each limit uses
one of the 64 shared meters of the switch. RTLPlayground gives every limit
that is set its own meter, so one port's storm does not eat into the limit of
another. Meters are handed out from 63 downwards when a limit is set and given
back when it is turned off; a map of which meters are in use (`rtl837x_meter.c`)
keeps storm control from taking a meter something else uses. The meter of a
port and type is the 6 bit field `port % 5` of its `RTL837X_STORM_MIDX` word.
The limit of a meter is in `RTL837X_METER_RATE + 4 * meter`, and bit
`meter % 32` of `RTL837X_METER_MODE + 4 * (meter / 32)` selects pps instead of
kbit/s.
```
#define RTL837X_STORM_CTRL		0x54e4	/* + 4 * type, bit = port */
#define RTL837X_STORM_MIDX		0x54f4	/* + 8 * type + 4 * (port / 5), 6 bits per port */
#define RTL837X_METER_RATE		0x5cf0	/* + 4 * meter, 24 bits, kbit/s or pps */
#define RTL837X_METER_MODE		0x5ef0	/* + 4 * (meter / 32), bit set = pps */
```
In kbit/s mode one step of the rate is 1 kbit/s. Measured with 100 and 1000 byte
frames, the meter counts a frame with its FCS, but without preamble and
interframe gap. Every meter has a bit that adds them, in `0x5f08 + 4 * (meter
/ 32)`, which storm control leaves cleared. `IGBW_INC_IFG` and `EGBW_INC_IFG`
are the matching bits of the bandwidth control.

An ingress bandwidth limit on the same port works on the same arriving frames
as the storm limit rather than after it, and the two together pass far less
than either one: a 1 Mbit/s `umcast` limit with a 10 Mbit/s ingress limit let
0.12 to 0.16 Mbit/s of a 1.4 Gbit/s stream through, where each alone passed
1.17 and 10.2 Mbit/s.

The type is 0 for broadcast, 1 for multicast, 2 for unknown unicast and 3 for
unknown multicast. The burst size of a meter (0x5df0 + 4 * meter) counts packets
in pps mode; its default of 0x2000 lets 8192 frames through before a pps limit
bites, so a pps limit sets the burst to one second of traffic. A kbit/s limit
puts back the default.

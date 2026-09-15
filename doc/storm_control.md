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
`umcast` is multicast without an entry in the address table. A limit in
packets per second goes from 1 to 1048575, a limit in kbit/s from 16 to
10000000 and is rounded down to steps of 16 kbit/s. `storm` without arguments
prints the limits of every port. The limits are kept in the configuration like
any other setting, and the Bandwidth page of the web interface has a Storm
control card to set them.

## Registers
Each port and type has an enable bit and a meter index, and each limit uses
one of the 64 shared meters of the switch. RTLPlayground gives every port and
type its own meter, index `port * 4 + type`, so one port's storm does not eat
into the limit of another.
```
#define RTL837X_STORM_CTRL		0x54e4	/* + 4 * type, bit = port */
#define RTL837X_STORM_MIDX		0x54f4	/* + 8 * type + 4 * (port / 5), 6 bits per port */
#define RTL837X_METER_RATE		0x5cf0	/* + 4 * meter, 24 bits */
#define RTL837X_METER_MODE		0x5ef0	/* + 4 * (meter / 32), bit set = pps */
```
The type is 0 for broadcast, 1 for multicast, 2 for unknown unicast and 3 for
unknown multicast. The burst size of a meter (0x5df0 + 4 * meter) counts packets
in pps mode; its default of 0x2000 lets 8192 frames through before a pps limit
bites, so a pps limit sets the burst to one second of traffic. A kbit/s limit
puts back the default.

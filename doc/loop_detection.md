# Loop Detection
The RTL837x can find loops on its own ports with RLDP. While it is on, the
switch sends a test frame carrying its MAC address out of every checked port
and watches for it to come back. A frame that returns means the port is part
of a loop, for example two ports cabled to the same unmanaged switch, or a
cable plugged back into the switch itself.

RLDP only finds the loop. RTLPlayground then blocks the port: it stops the
port's MAC from sending and receiving, so the link stays up but no frames pass.
Of two ports that loop into each other only the higher numbered one is blocked.
After 60 seconds the port is opened again and checked; if the loop is still
there it is blocked again.

Loop detection runs next to Spanning Tree and catches loops STP cannot see,
such as on ports with STP switched off or behind a device that drops BPDUs.
It leaves STP's decisions alone: test frames only go out of ports that STP
forwards on, and a port is not blocked when the other end of its loop is a
port STP does not forward on, since STP has already broken that loop. Without
this a redundant uplink that STP keeps blocked shows up as a loop on the
forwarding uplink. A port can be left out of the check.

## Commands
```
rldp on|off
rldp <port> on|off
rldp
```
`rldp on` starts sending test frames on the forwarding ports, `rldp <port> off` leaves one
port out of the check (and opens it if it was blocked), and `rldp` prints the
state of every port. The settings are kept in the configuration. The Spanning
Tree page of the web interface has a Loop detection card with the same
settings, and `/rldp.json` returns the state.

## Registers
```
#define RTL837X_RLDP_RLPP		0x106c	/* bit 0 enable, bit 2 compare ID, bit 5 periodic */
#define RTL8373_RLDP_TIMER		0x1074	/* check and loop intervals */
#define RTL837X_RLDP_TX_PMSK		0x1078	/* ports that send test frames */
#define RTL837X_RLDP_MAGIC0		0x1084	/* magic value, low 32 bits */
#define RTL837X_RLDP_MAGIC1		0x1088	/* magic value, high 16 bits */
#define RTL837X_RLDP_LOOP_STATE		0x108c	/* bit = port in a loop */
#define RTL837X_RLDP_LOOPPAIR		0x1098	/* + 4 * (port / 8), 4 bits per port */
#define RTL837X_MAC_L2_PORT_CTRL	0x1238	/* + 0x100 * port, bit 0 RX, bit 1 TX */
#define RTL837X_MSTP_STATES		0x5310	/* 2 bits per port, 3 = forwarding */
```
The switch is set to send test frames periodically and to compare only the
magic value, which is the switch's MAC address. The random ID the switch also
puts into the frames is regenerated whenever the firmware asks for a random
number, so comparing it could miss a loop.

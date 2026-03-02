# Egress and Ingress Bandwidth Control
The RTL8372/3 allows to control the bandwidth of data transmitted (egress) and/or
admitted (ingress) at any given port. Once admitted, packets are internally switched
at wire-speed, since the backplane of the devices has a bandwidth of 60GBit/s.

The devices schedules transmission of packets by assigning packets to 8 queues
implemented in hardware per port, which share a total of 8Mbit of memory internal
to the switching part of the SoCs. Packets are assigned to the respective queues
based on the priority assigned to a packet, which can be based on various
properties of a packet such as  IEEE 802.1P priority, DSCP value, physical port
number, destination or source MAC, Ether-Type-based, CVID, SVID, IPv4 source or
destination IP, IPv4/IPv6 TOS field, IPv6 Flow Label and even TCP/UDP
source/destination port. Once in a queue, packets are scheduled for egress
based on differnent configurable algorithms.

RTLPlayground currently allows only to control the bandwidth at ingress at a port
or just before packets leave a port. There is no control of the priority assignment
or queue scheduling mechanisms. The bandwidth can be controlled in steps of 16Kbit/s
from 16Kbit/s to 10Gbp/s.

The bandwidth control as currently implemented allows e.g. to assign a certain
share of bandwidth to an attached device (e.g. to share an uplink), or simulate
connections with low bandwidth and even bad connectivity with packet drops when
ingress is not controlled by Flow Control but by simply droping packets.

## Ingress/Egress control
The relevant registers for controlling Ingress and Egress at a port are:
```
#define RTL837X_IGBW_CTRL		0x4c10
#define IGBW_INC_BYPASS_PKT		0x100
#define IGBW_INC_IFG			0x80
#define IGBW_ADM_DHCP			0x20
#define IGBW_ADM_ARPREQ			0x10
#define IGBW_ADM_RMA			0x08
#define IGBW_ADM_BPDU			0x04
#define IGBW_ADM_RTKPKT			0x02
#define IGBW_ADM_IGMP			0x01
#define RTL837X_IGBW_PORT_CTRL		0x4C18
#define RTL837X_IGBW_PORT_FC_CTRL	0x4C8C
#define RTL837X_EGBW_PORT_CTRL		0x1c34
#define RTL837X_EGBW_CTRL		0x447c
#define EGBW_INC_IFG			0x02
#define EGBW_CPUMODE			0x01
```
`RTL837X_IGBW_CTRL/RTL837X_EGBW_CTRL` control the behaviour of the bandwidth control
at ingress and egress. The flags such as `IGBW_ADM_DHCP`control whether certain types
of packets such as DHCP are exempt from being ingress controlled. The
`IGBW_INC_IFG/EGBW_INC_IFG` flags control whether the Inter Frame Gaps are part of
the bandwidth being controlled. `EGBW_CPUMODE` controls whether packets generated
by the internal CPU are subject to egress control.

`RTL837X_IGBW_PORT_CTRL/RTL837X_EGBW_PORT_CTRL` configure the bandwidth for ingress
and egress at a port.

`RTL837X_IGBW_PORT_FC_CTRL` configures whether packets are bandwidth-controlled using
Flow Control (port-bit set), or simply dropped (port-bit clear).

## Ingress/Egress bandwidth API
The code currently provides the following functions:
```
void bandwidth_setup(void) __banked;
void bandwidth_ingress_set(uint8_t port, __xdata uint32_t bw) __banked;
void bandwidth_ingress_disable(uint8_t port) __banked;
void bandwidth_ingress_drop(uint8_t port) __banked;
void bandwidth_egress_set(uint8_t port, __xdata uint32_t bw) __banked;
void bandwidth_egress_disable(uint8_t port) __banked;
void bandwidth_status(uint8_t port) __banked;
```c

`bandwidth_setup()` is called at boot-time and configures excluding all special packets
that may be for the CPU and packets outgoing from the CPU to be excluded from bandwidth
control. IFG is not part of the bandwidth calculation.

`bandwidth_ingress_set()` enables ingress bandwidth control for a particular port given
the specified bandwidth. This also enabled Flow Control at a port.

`bandwidth_ingress_set()` enables egress bandwidth control for a particular port given
the specified bandwidth

`bandwidth_ingress_disable() / bandwidth_egress_disable()` disable ingress and egress
bandwidth control at a given port

`bandwidth_ingress_drop(port)` configures packets exceeding bandwidth limitations to
simply be dropped

`bandwidth_status(port)` shows the current bandwidth control status for a given port

## Bandwidth control configuration on the Serial Console
The following commands are provided on the serial console:
```
> bw [in|out|status] <port> [<hexvalue>|off|drop]
  Configures or shows the status of bandwidth control
```
The bandwidth is given as the `<hexvalue>` in Kbit/s. Note that the control is only
possible at a granularity of 16 Kbit/s and the minimum value is also 16 Kbit/s. The
hexadecimal numbers must be given in full bytes, i.e. have an even number of digits.

To enable bandwidth control of ingress for physical port 2 to be set to 256 Kbit/s
do:
```
> bw in 2 0100 
```

To drop packets when the bandwidth is exceeeded at port 2 do:
```
> bw in 2 drop 
```

To disable bandwidth control for incoming packets on port 2 do:
```
> bw in 2 off
```

## Bandwidth configuration via the Web Interface
Not implemented, yet!

## A Test using iperf3
The following is and example how to test bandwidth control with a signle Linux device using
network namespaces to route packets between a client and a server on the same Linux device
through an external switch.

You will need 2 network intefaces on the linux device, say, 2 USB-Ethernet controllers called
eth0 and eth1: 
```
$ sudo ip netns add client
$ sudo ip netns add server

$ sudo ip link set dev eth0 netns client
$ sudo ip link set dev eth1 netns server

$ sudo ip netns exec client ip link set dev eth0 up
$ sudo ip netns exec server ip link set dev eth1 up

$ sudo ip netns exec client ip addr add dev eth0 192.168.99.1/24
$ sudo ip netns exec server ip addr add dev eth1 192.168.99.2/24

$ sudo ip netns exec server iperf3 -s
```
This will start an iper3 server in the above shell.

In a different shell you can now run the iperf3 client against your server:
```
$ sudo ip netns exec client iperf -c 192.168.99.2
```
The LEDs on your switch where your network adapters are connected should start to flicker.
On a 1GBit connection, you should see:
```
$ sudo ip netns exec client iperf3 -c 192.168.99.2
Connecting to host 192.168.99.2, port 5201
[  5] local 192.168.99.1 port 46776 connected to 192.168.99.2 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   114 MBytes   952 Mbits/sec    0    339 KBytes       
[  5]   1.00-2.00   sec   113 MBytes   946 Mbits/sec    0    356 KBytes       
[  5]   2.00-3.00   sec   112 MBytes   937 Mbits/sec    0    390 KBytes       
[  5]   3.00-4.00   sec   112 MBytes   942 Mbits/sec    0    390 KBytes       
[  5]   4.00-5.00   sec   112 MBytes   943 Mbits/sec    0    390 KBytes       
[  5]   5.00-6.00   sec   112 MBytes   944 Mbits/sec    0    390 KBytes       
[  5]   6.00-7.00   sec   112 MBytes   938 Mbits/sec    0    390 KBytes       
[  5]   7.00-8.00   sec   112 MBytes   942 Mbits/sec    0    410 KBytes       
[  5]   8.00-9.00   sec   113 MBytes   947 Mbits/sec    0    410 KBytes       
[  5]   9.00-10.00  sec   112 MBytes   940 Mbits/sec    0    410 KBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  1.10 GBytes   943 Mbits/sec    0            sender
[  5]   0.00-10.00  sec  1.10 GBytes   941 Mbits/sec                  receiver
```

Now, we limit ingress on port 1 (connected to eth0) to 4 MBit/s:
```> bw in 1 1000
bandwidth_ingress_set called, port 04
RTL837X_IGBW_PORT_CTRL:0x00100100
RTL837X_IGBW_PORT_FC_CTRL:0x00000010
```

We now get:
```
$ sudo ip netns exec client iperf3 -c 192.168.99.2
[  5] local 192.168.99.1 port 43324 connected to 192.168.99.2 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec  1.12 MBytes  9.43 Mbits/sec    0    160 KBytes       
[  5]   1.00-2.00   sec   640 KBytes  5.24 Mbits/sec    0    160 KBytes       
[  5]   2.00-3.00   sec   384 KBytes  3.15 Mbits/sec    0    160 KBytes       
[  5]   3.00-4.00   sec   384 KBytes  3.15 Mbits/sec    0    160 KBytes       
[  5]   4.00-5.00   sec   640 KBytes  5.24 Mbits/sec    0    160 KBytes       
[  5]   5.00-6.00   sec   256 KBytes  2.10 Mbits/sec    0    160 KBytes       
[  5]   6.00-7.00   sec   640 KBytes  5.24 Mbits/sec    0    160 KBytes       
[  5]   7.00-8.00   sec   384 KBytes  3.15 Mbits/sec    0    160 KBytes       
[  5]   8.00-9.00   sec   640 KBytes  5.24 Mbits/sec    0    160 KBytes       
[  5]   9.00-10.00  sec   256 KBytes  2.10 Mbits/sec    0    160 KBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  5.25 MBytes  4.40 Mbits/sec    0            sender
[  5]   0.00-10.16  sec  4.75 MBytes  3.92 Mbits/sec                  receiver

iperf Done.
```
Which is the 4Mbit/s we configured. There are no packet drops (retries) because
Flow Control is used to signal the Ethernet adapter on the incoming interface
(port 1 of the router) to slow down.

We can also configure a mere 256KBit/s and packet drop to simulate a bad connection:
```
> bw in 1 0100
bandwidth_ingress_set called, port 04
RTL837X_IGBW_PORT_CTRL:0x00100010
RTL837X_IGBW_PORT_FC_CTRL:0x00000010

> bw in 1 drop
RTL837X_IGBW_PORT_FC_CTRL:0x00000000
```

We now get:
```
$ sudo ip netns exec client iperf3 -c 192.168.99.2
Connecting to host 192.168.99.2, port 5201
[  5] local 192.168.99.1 port 46060 connected to 192.168.99.2 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   384 KBytes  3.14 Mbits/sec    2   1.41 KBytes       
[  5]   1.00-2.00   sec  0.00 Bytes  0.00 bits/sec   54   1.41 KBytes       
[  5]   2.00-3.00   sec  0.00 Bytes  0.00 bits/sec   31   29.7 KBytes       
[  5]   3.00-4.00   sec  0.00 Bytes  0.00 bits/sec    2   1.41 KBytes       
[  5]   4.00-5.00   sec  0.00 Bytes  0.00 bits/sec   23   1.41 KBytes       
[  5]   5.00-6.00   sec   128 KBytes  1.05 Mbits/sec   16   14.1 KBytes       
[  5]   6.00-7.00   sec  0.00 Bytes  0.00 bits/sec    2   1.41 KBytes       
[  5]   7.00-8.00   sec  0.00 Bytes  0.00 bits/sec   11   1.41 KBytes       
[  5]   8.00-9.00   sec   128 KBytes  1.05 Mbits/sec    9   8.48 KBytes       
[  5]   9.00-10.00  sec  0.00 Bytes  0.00 bits/sec    2   1.41 KBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec   640 KBytes   524 Kbits/sec  152            sender
[  5]   0.00-10.00  sec   256 KBytes   210 Kbits/sec                  receiver

iperf Done.
```
Which shows a large number of retries due to dropped packets and an average number
of received packets (the client sends the packets to the server, and they are sent
back to the client by the server) of 210 KBit/s, the number is higher for the transmitted
packets, because they may include dropped packets.

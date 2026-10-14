# Priority and Queue Scheduling
Every port has 8 egress queues, described together with the shared queue memory in
[bandwidth.md](bandwidth.md). A packet is assigned an internal priority of 0 to 7
when it is admitted, the internal priority selects the queue it is put into, and a
scheduler per port decides in which order the queues of that port are served.

The internal priority can be derived from several properties of a packet. Each
source has its own 5-bit weight field in `RTL837X_PRI_WEIGHT`, and none of them is
written until a source is switched on. The two tables in between are left as they
are, so with the identity mapping an RTL8373 holds in them, 802.1p priority n
becomes internal priority n and internal priority n goes to queue n.

RTLPlayground drives the 802.1p source and the scheduler weights. The DSCP source,
priority and DSCP remarking, and rate control per queue are not implemented.

## Registers
```
#define RTL837X_SCHED_PORT_Q_CTRL	0x1d28
#define SCHED_Q_STRICT			0x80
#define RTL837X_DOT1Q_PRI_REMAP		0x5174
#define PRI_REMAP_MASK			0x07
#define RTL837X_PRI_WEIGHT		0x5198
#define PRI_WEIGHT_MASK			0x1f
#define PRI_WEIGHT_DOT1Q		0x01
```
`RTL837X_PRI_WEIGHT` holds one 5-bit weight per priority source, the 802.1p source
in its lowest five bits. A second copy of the word follows at `0x519c`; which of the
two a port uses is selected by a bit per port in `PORT_WEIGHT_SEL` at `0x51a0`, and
that register is zero, so all ports read the first copy.

`RTL837X_DOT1Q_PRI_REMAP` maps the 802.1p priority of a tagged packet to an internal
priority, three bits per 802.1p value on a nibble boundary, the value 0 in the least
significant nibble.

`RTL837X_SCHED_PORT_Q_CTRL` is the scheduler entry of one queue, at
`0x1d28 + port * 1024 + queue * 4`. Its lowest byte carries the weight of the queue
in bits 0 to 6, and `SCHED_Q_STRICT` in bit 7 marks the queue as strict priority
instead of giving it a share of the weighted round robin.

The remaining registers of the block are left as they are: the internal priority to
queue table at `0x51a4 + port * 4`, the port-based source at `0x5170`, the DSCP table
at `0x5178`, and the scheduler algorithm bit per port at `0x4534`.

## API
```
void qos_dot1q_set(uint8_t on) __banked;
void qos_pcp_set(uint8_t pcp, uint8_t priority) __banked;
void qos_queue_set(uint8_t port, uint8_t queue, uint8_t weight) __banked;
void qos_show(void) __banked;
```
`qos_dot1q_set()` gives the 802.1p source a weight, or takes it away again.

`qos_pcp_set()` points one 802.1p priority at an internal priority.

`qos_queue_set()` writes the scheduler entry of one queue of one port, either a
weight from 1 to `QOS_WEIGHT_MAX` or `SCHED_Q_STRICT`.

`qos_show()` prints whether the 802.1p source has a weight, the 802.1p table, and the
weights of all queues of every port.

## Configuration on the Serial Console
```
> qos
  Shows the 802.1p mapping and the queue weights of every port
> qos on|off
  Lets the 802.1p priority of a packet decide its queue, or stops it from deciding
> qos pcp <pcp> <priority>
  Maps one 802.1p priority to an internal priority, both 0 to 7
> qos queue <port> <queue> <weight>|sp
  Sets the scheduler weight of a queue, 1 to 127, or strict priority
```
To put voice traffic tagged with 802.1p priority 5 into a queue that is served before
the others, and let the rest share the port in equal parts:
```
> qos on
> qos queue 1 5 sp
```
To give queue 2 of port 1 three times the share of the other queues instead:
```
> qos queue 1 2 3
> qos queue 1 0 1
```

## Configuration via the Web Interface
Not implemented, yet!

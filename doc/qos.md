# Priority and Queue Scheduling
Every port has 8 egress queues, described together with the shared queue memory in
[bandwidth.md](bandwidth.md). A packet is assigned an internal priority of 0 to 7
when it is admitted, the internal priority selects the queue it is put into, and a
scheduler per port decides in which order the queues of that port are served.

The internal priority can be derived from several properties of a packet, and each
source has its own 5-bit weight field in `RTL837X_PRI_WEIGHT`. A source counts for
nothing until its weight is written, and the weight is also what decides between
two sources that both apply to one packet. RTLPlayground writes the weight 1 when
it switches a source on, so with the 802.1p and the DSCP source both on, which of
the two a tagged IP packet follows is left to the switch; the commands do not set
a weight of their own yet.

Two more tables sit between a source and a queue, one from the property to the
internal priority and one from the internal priority to the queue. An RTL8373
holds the 802.1p table and the internal priority table as identity mappings, so
802.1p priority n becomes internal priority n and goes to queue n. The DSCP table
is the exception: it comes up all zero, which would send every DSCP value to
internal priority 0, so RTLPlayground fills it with the class selector mapping,
DSCP shifted right by three. That happens at startup and before any `qos ...` line
of the startup configuration is replayed, so a line that changes one entry keeps
its effect whatever order the lines come in.

RTLPlayground drives the 802.1p and DSCP sources, the scheduler weights and the
rate limiters of every queue. The port-based source and priority and DSCP
remarking are not implemented.

## Registers
```
#define RTL837X_SCHED_PORT_Q_CTRL	0x1d28
#define SCHED_Q_STRICT			0x80
#define RTL837X_DOT1Q_PRI_REMAP		0x5174
#define PRI_REMAP_MASK			0x07
#define RTL837X_DSCP_PRI_REMAP		0x5178
#define RTL837X_EGBW_PORT_Q_MAX		0x1c3c
#define RTL837X_EGBW_PORT_Q_ASSURED	0x1ca0
#define RTL837X_PRI_WEIGHT		0x5198
#define PRI_WEIGHT_MASK			0x1f
#define PRI_WEIGHT_DOT1Q		0x01
#define PRI_WEIGHT_DSCP_MASK		0x7c
#define PRI_WEIGHT_DSCP			0x04
```
`RTL837X_PRI_WEIGHT` holds one 5-bit weight per priority source, the 802.1p source
at bits 0 to 4 and the DSCP source at bits 10 to 14. `PRI_WEIGHT_DOT1Q` and
`PRI_WEIGHT_MASK` are therefore values within the first byte of the word,
`PRI_WEIGHT_DSCP` and `PRI_WEIGHT_DSCP_MASK` within the second. A second copy of
the word follows at `0x519c`; which of the two a port uses is selected by a bit per
port in `PORT_WEIGHT_SEL` at `0x51a0`, and that register is zero, so all ports read
the first copy.

`RTL837X_DOT1Q_PRI_REMAP` maps the 802.1p priority of a tagged packet to an internal
priority, three bits per 802.1p value on a nibble boundary, the value 0 in the least
significant nibble.

`RTL837X_DSCP_PRI_REMAP` does the same for the 6-bit DSCP field, but packs ten
values of three bits into a word: the word of a DSCP value is at
`0x5178 + dscp / 10 * 4` and its field starts at bit `dscp % 10 * 3`. Seven words
cover the 64 values and the last one carries four of them. Two fields of every word
cross a byte boundary, the one at index 2 over bits 6 to 8 and the one at index 5
over bits 15 to 17, so they cannot be reached with a single `sfr_mask_data()`.

`RTL837X_SCHED_PORT_Q_CTRL` is the scheduler entry of one queue, at
`0x1d28 + port * 1024 + queue * 4`. Its lowest byte carries the weight of the queue
in bits 0 to 6, and `SCHED_Q_STRICT` in bit 7 marks the queue as strict priority
instead of giving it a share of the weighted round robin.

A queue has two rate limiters of its own. `RTL837X_EGBW_PORT_Q_MAX` at
`0x1c3c + port * 1024 + queue * 8` caps what the queue may send and
`RTL837X_EGBW_PORT_Q_ASSURED` at `0x1ca0 + port * 1024 + queue * 4` is the rate
it is given before the other queues are served. Both hold a rate in the lowest
20 bits and `BW_LB_EN` in bit 20, the same layout as the per-port limiter in
[bandwidth.md](bandwidth.md), so `bandwidth_rate_set()` writes all three. Note
the stride: the maximum takes two words per queue, the second one its burst,
while the assured rate takes one.

The remaining registers of the block are left as they are: the internal priority to
queue table at `0x51a4 + port * 4`, the port-based source at `0x5170`, and the
scheduler algorithm bit per port at `0x4534`.

## API
```
void qos_defaults(void) __banked;
void qos_dot1q_set(uint8_t on) __banked;
void qos_dscp_weight_set(uint8_t on) __banked;
void qos_pcp_set(uint8_t pcp, uint8_t priority) __banked;
void qos_dscp_set(uint8_t dscp, uint8_t priority) __banked;
void qos_queue_set(uint8_t port, uint8_t queue, uint8_t weight) __banked;
void qos_queue_max_set(uint8_t port, uint8_t queue, __xdata uint32_t bw) __banked;
void qos_queue_min_set(uint8_t port, uint8_t queue, __xdata uint32_t bw) __banked;
void qos_show(void) __banked;
```
`qos_defaults()` writes the class selector mapping into the DSCP table.

`qos_dot1q_set()` gives the 802.1p source a weight, or takes it away again, and
`qos_dscp_weight_set()` does the same for the DSCP source.

`qos_pcp_set()` points one 802.1p priority at an internal priority, `qos_dscp_set()`
one DSCP value.

`qos_queue_set()` writes the scheduler entry of one queue of one port, either a
weight from 1 to `QOS_WEIGHT_MAX` or `SCHED_Q_STRICT`.

`qos_queue_max_set()` and `qos_queue_min_set()` set the two rate limiters of one
queue; a rate of 0 switches the limiter off.

`qos_show()` prints which of the two sources have a weight, both of their tables,
the weights of all queues of every port, and every queue limiter that is switched
on.

## Configuration on the Serial Console
```
> qos
  Shows both mappings, which sources have a weight, the queue weights of
  every port, and the queue limits that are set
> qos on|off
  Lets the 802.1p priority of a packet decide its queue, or stops it from deciding
> qos dscp on|off
  Lets the DSCP field of a packet decide its queue, or stops it from deciding
> qos pcp <pcp> <priority>
  Maps one 802.1p priority to an internal priority, both 0 to 7
> qos dscp <dscp> <priority>
  Maps one DSCP value to an internal priority, 0 to 63 and 0 to 7
> qos queue <port> <queue> <weight|sp>
  Sets the scheduler weight of a queue, 1 to 127, or strict priority
> qos max <port> <queue> <hexvalue|off>
  Caps a queue, the rate in Kbit/s as for bw, or lifts the cap
> qos min <port> <queue> <hexvalue|off>
  Gives a queue a rate before the others are served, or takes it away
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
The class selector mapping already puts DSCP 46, the code point of expedited
forwarding, in queue 5, so letting the DSCP field decide takes one line:
```
> qos dscp on
```
To send that one code point to queue 6 and leave the rest of the mapping alone:
```
> qos dscp 46 6
```

A queue with a small weight gets little of a busy port. An assured rate gives it
a rate of its own whatever the weights, here 4 Mbit/s for queue 0 of port 1:
```
> qos queue 1 5 127
> qos min 1 0 0fa0
```

## Configuration via the Web Interface
Not implemented, yet!

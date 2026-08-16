# The health command

`health` prints a one-page snapshot of the firmware's vital signs on the
serial console. It exists for the situations where the switch misbehaves and
every probe from the network measures the network instead of the device: the
dump is taken from inside, costs one command, and works while HTTP does not.

```
> health
up 0000001f ticks 0x00001f4c loops 0x000016da rx 0x024e
link max 01 slow 0x0000
sfp  max 74 slow 0x0001
rx   max 00 slow 0x0000
tx   max 00 slow 0x0000
stp  max 04 slow 0x0001
cmd  max 02 slow 0x0001
sp 80 untouched 62
tcp st 00 tmr 00 rtx 00 rto 00 len 0x0000 mss 0x05b4
  lport 0x0000 rport 0x0000 rip 0x0000 0x0000
httpd left 0x0000 entry 00 stp 01 mvlan 0x0002
```

All figures are raw hex. The counters are cumulative: take two dumps a known
time apart and the differences give the rates.

## The first line

`up` is the seconds counter, `ticks` the system tick (200 per second),
`loops` the number of main-loop passes and `rx` the frames handle_rx has
taken off the NIC. A healthy loop completes one pass per tick, so the
difference in `loops` tracks the difference in `ticks`; a loop that falls
behind is being held up by one of the phases below. The `rx` rate separates
a flooded CPU port from a stalled loop, which look identical from outside.

## Phases

Each line is one section of the main loop: the link poll, SFP handling,
frame reception, TCP transmission, the STP timers and the command dispatch.
`max` is the worst time that section has taken, in ticks of 5 ms; `slow`
counts the passes that took two ticks or more. A misbehaving subsystem
names itself here, which is the difference between knowing that the loop is
slow and knowing why.

A dump of its own output takes the console some milliseconds, so running
`health` repeatedly increments the `cmd` counters by itself. That is the
instrument observing itself, not a fault.

## Stack

At boot the free stack area is painted with a pattern. `sp` is the live
stack pointer; `untouched` counts the painted bytes still intact at the top,
which is the closest thing to a high-water mark this hardware offers. A
shrinking `untouched` across dumps means something is reaching deeper than
anything before it.

## The TCP slot

The connection table is printed whole, one slot per pair of lines: TCP
state (3 is ESTABLISHED, 0 closed), retransmission timer and counter, bytes
in flight, negotiated MSS, then the ports and the peer address. With a
single slot serving the entire web interface, one glance answers the
question every management outage starts with: who is holding it. A slot
showing state 3 with nothing in flight and a peer that no longer answers is
a connection whose owner went away.

`httpd left` and `entry` describe the file transfer the web server believes
it is in the middle of, and the trailing flags show whether STP is running
and which VLAN carries management.

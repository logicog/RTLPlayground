# sFlow

The switch can send sFlow version 5 counter samples to a collector, so that
traffic and error counters of every port can be graphed and kept over time
somewhere else, and flow samples, the headers of frames picked by the ASIC,
so that the collector can tell what the traffic is made of.

```
sflow collector <ip> [port]   # where to send, default port 6343
sflow interval <1-3600>       # seconds between two samples of one port, default 20
sflow sampling <0|100-65535>  # sample 1 in N received frames, default 0 (off)
sflow on                      # start sending
sflow off
sflow                         # show the settings, the datagrams sent and the
                              # samples dropped; "waiting" means on without a
                              # collector or without a free UDP connection
```

The same settings are on the sFlow page of the web interface, which also shows
whether the agent is sending and how many datagrams went out. Settings made
there, like those typed on the console, are kept only once the configuration
is saved.

`sflow on` may come before `sflow collector` in the saved configuration: the
agent starts as soon as both are set.

Every datagram carries one counters sample for one port, and the ports take
turns, so a switch with 9 ports and the default interval sends a datagram
about every two seconds. The agent address is the switch's own IP address,
and the data source of each sample is the front panel port number as ifIndex.

Each sample holds two records:

* generic interface counters: ifSpeed and link status, octets, unicast,
  multicast and broadcast packets, discards and errors in both directions;
* Ethernet interface counters: FCS errors, single, multiple, late and
  excessive collisions, deferred transmissions, frames too long and symbol
  errors.

All values come from the MIB counters of the ASIC. Counters it does not keep
(alignment errors, SQE test errors, internal MAC errors, carrier sense errors,
unknown protocols) are sent as 0xffffffff, which sFlow defines as unknown.

## Flow samples

With `sflow sampling N` the ASIC picks one in N frames received on any port
and copies it to the CPU port, using its port mirroring with the sample rate
in register 0x50ec. The register takes one less than N: 0 copies every frame,
1 every second one, 10 one in eleven, as measured on the hardware. The
firmware sends the first 128 bytes of each such frame as a raw packet header
record, with the input port, the sampling rate and the number of frames
received on that port as the sample pool. One sample waits at a time and goes
out with the next tick, so at most 200 per second are sent; samples arriving
before that are counted as drops and reported in every flow sample. A counter
sample that is due goes first, so sampled frames cannot hold the counters
back.

The lowest rate is 1 in 100. Every copy takes a slot of the receive ring
the CPU shares with all frames addressed or trapped to it, and the firmware
drains a few frames per pass of its main loop. With a lower rate one busy
port would fill the ring with copies, management traffic and the protocol
frames of STP and LACP would be lost with them, and a saved configuration
would do so again at every boot. The limit does not make line rate on a port
with small frames safe either; pick the rate for the traffic you expect.

The rate is shared by all ports. Port mirroring has only one session, so the
`mirror` command and flow sampling exclude each other: while sampling is on,
`mirror` is refused, and `sflow sampling` is refused while a mirror session
is set up. Turn the other one off first.

The ASIC marks each mirrored copy with its own reason code, 0x3a, in the CPU
tag. The firmware hands exactly these copies to the sampler, so broadcast and
multicast frames are sampled as well, while the original frame is handled as
usual. Frames addressed to the switch itself and frames trapped to the CPU,
like LACPDUs, are not mirrored and therefore never sampled.

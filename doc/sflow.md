# sFlow

The switch can send sFlow version 5 counter samples to a collector, so that
traffic and error counters of every port can be graphed and kept over time
somewhere else. Flow samples (sampled packet headers) are not sent.

```
sflow collector <ip> [port]   # where to send, default port 6343
sflow interval <1-3600>       # seconds between two samples of one port, default 20
sflow on                      # start sending
sflow off
sflow                         # show the settings and the datagrams sent
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

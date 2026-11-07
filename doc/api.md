## HTTP API

The switch firmware implements a JSON API which underpins the webUI.
Port numbers referred to throughout the API are physical port numbers
and should reflect the physical numbers visible on the switch.

The following endpoints are supported:
 * /bandwidth.json
 * /counters.json?port=<PORTNUM>
 * /eee.json
 * /information.json
 * /isolation.json
 * /l2.json
 * /lag.json
 * /lldp.json
 * /mirror.json
 * /status.json
 * /stp.json
 * /stpcnt.json
 * /storm.json
 * /vlan.json?vid=<VLANID>
 * /vlanlist.json



## bandwidth
```json
[
    {"port":1,"iLimited":false,"iBW":"0x0fffff","iAction":"drop","eLimited":false,"eBW":"0x0fffff"},
    {"port":2,"iLimited":false,"iBW":"0x0fffff","iAction":"fc","eLimited":false,"eBW":"0x0fffff"},
    ...
]
```
Array of port properties


| Property | Description
|----------|------------
| `port` | The physical port number
| `iLimited` | Apply ingress (inbound) limit
| `iBW` | The ingress rate limit applied
| `iAction` | Action to preform on limit exceedence, "drop" - drop frame "fc" - use TCP flow control
| `eLimited` | Apply egress (outbound) limit
| `eBW` | The egress rate limit applied

## counters
GET parameter `port=<PORTNUM>`
Array of counters for the specified port
```json
    ["0x22cc9","0x51b02","0x2d4","0x1f","0x164","0x262","0x17b","0x0","0x6","0x0","0x0","0x0","0x0","0x0","0x17b","0x0","0x0","0x0","0x0","0x0","0x2b1","0x1f40000002a","0x7800000008","0x1620000004e","0x8400000008","0x5000000a4","0x0","0x0","0x0","0x3dd","0x45700000000","0x3dd","0x45700000000","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x3dd","0x3dd","0x457","0x0","0x0","0x0","0x0","0x0","0x0","0x0"]
```
| Index | Description
| ----|--------------------
| 0 | Interface-in Octets (64-bit)
| 1 | Interface-out Octets (64-bit)
| 2 | Interface-in Unicast Pkts (64-bit)
| 3 | Interface-in Multicast Pkts (64-bit)
| 4 | Interface-in Broadcast Pkts (64-bit)
| 5 | Interface-out Unicast Pkts (64-bit)
| 6 | Interface-out Multicast Pkts (64-bit)
| 7 | Interface-out Broadcast Pkts (64-bit)
| 8 | Upper 32-bits: Interface out discards; lower 32-bits: 802.1d Tp Port in discards
| 9 | Upper 32-bits: 802.3 Single collision frames; lower 32-bits: 802.3 Multi collision frames
| 10 | Upper 32-bits: 802.3 Deferred transmissions; lower 32-bits: 802.3 Late collisions
| 11 | Upper 32-bits: 802.3 Excessive collisions; lower 32-bits: 802.3 Symbol errors
| 12 | Upper 32-bits: 802.3 Control in unknown opcodes; lower 32-bits: 802.3 In Pause frames
| 13 | Upper 32-bits: 802.3 Out Pause frames; lower 32-bits: Ether drop events
| 14 | Upper 32-bits: TX Ether Broadcast Pkts; lower 32-bits: TX Ether Multicast Pkts
| 15 | Upper 32-bits: TX Ether CRC Align errors; lower 32-bits: RX Ether CRC Align errors
| 16 | Upper 32-bits: TX Ether Undersized Pkts; lower 32-bits: RX Ether Undersized Pkts
| 17 | Upper 32-bits: TX Ether Oversized Pkts; lower 32-bits: RX Ether Oversized Pkts
| 18 | Upper 32-bits: TX Ether Fragments; lower 32-bits: RX Ether fragments
| 19 | Upper 32-bits: TX Ether Jabbers; lower 32-bits: RX Ether Jabbers
| 20 | Upper 32-bits: TX Ether Collisions; lower 32-bits: TX Ether Pkts 64 Octets
| 21 | Upper 32-bits: RX Ether Pkts 64 Octets; lower 32-bits: TX Ether 65-127 Octets
| 22 | Upper 32-bits: RX Ether 65-127 Octets; lower 32-bits: TX Ether Pkts 128-255 Octets
| 23 | Upper 32-bits: RX Ether Pkts 128-255 Octets; lower 32-bits: TX Ether Pkts 256-511 Octets
| 24 | Upper 32-bits: RX Ether Pkts 256-511 Octets; lower 32-bits: TX Ether Pkts 512-1023 Octets
| 25 | Upper 32-bits: RX Ether Pkts 512-1023 Octets; lower 32-bits: TX Ether Pkts 1024-1518 Octets
| 26 | Upper 32-bits: RX Ether Pkts 1024-1518 Octets; lower 32-bits: unlabeled counter
| 27 | Upper 32-bits: RX Ether Undersized Drop Pkts; lower 32-bits: TX Ether Pkts >1518 Octets
| 28 | Upper 32-bits: RX Ether Pkts >1518 Octets; lower 32-bits: TX Ether Pkts too large
| 29 | Upper 32-bits: RX Ether Pkts too large; lower 32-bits: TX Ether Flexible Octets Set 1
| 30 | Upper 32-bits: RX Ether Flexible Octets Set 1; lower 32-bits: TX Ether Flexible Octets CRC Set 1
| 31 | Upper 32-bits: RX Ether Flexible Octets CRC Set 1; lower 32-bits: TX Ether Flexible Octets Set 0
| 32 | Upper 32-bits: RX Ether Flexible Octets Set 0; lower 32-bits: TX Ether Flexible Octets CRC Set 0
| 33 | Upper 32-bits: RX Ether Flexible Octets CRC Set 0; lower 32-bits: Length Field Errors
| 34 | Upper 32-bits: False Carriers; lower 32-bits: Undersized Octets
| 35 | Upper 32-bits: Framing Errors; lower 32-bits: unlabeled counter
| 36 | Upper 32-bits: RX MAC Discards; lower 32-bits: RX MAC IPG Short Drop
| 37 | Upper 32-bits: unlabeled counter; lower 32-bits: 802.1d TP Learned Entry Discards
| 38 | Egress Queue 7 Dropped Pkts
| 39 | Egress Queue 6 Dropped Pkts
| 40 | Egress Queue 5 Dropped Pkts
| 41 | Egress Queue 4 Dropped Pkts
| 42 | Egress Queue 3 Dropped Pkts
| 43 | Egress Queue 2 Dropped Pkts
| 44 | Egress Queue 1 Dropped Pkts
| 45 | Egress Queue 0 Dropped Pkts
| 46 | Egress Queue 7 Out Pkts
| 47 | Egress Queue 6 Out Pkts
| 48 | Egress Queue 5 Out Pkts
| 49 | Egress Queue 4 Out Pkts
| 50 | Egress Queue 3 Out Pkts
| 51 | Egress Queue 2 Out Pkts
| 52 | Egress Queue 1 Out Pkts
| 52 | Egress Queue 1 Out Pkts
| 53 | Egress Queue 0 Out Pkts
| 54 | TX Good Counter (64-bit)
| 55 | RX Good Counter (64-bit)
| 56 | Upper 32-bits: RX Error Counter; lower 32-bits: TX Error Counter
| 57 | TX Good Counter PHY (64-bit)
| 58 | RX Good Counter PHY (64-bit)
| 59 | Upper 32-bits: RX Error Counter PHY; lower 32-bits: TX Error Counter PHY

## eee
```json
[
    {"port":1,"isSFP":false,"advertised":["2.5G","1G","100M"],"linkPartner":["1G","100M"],"active":true},
    {"port":2,"isSFP":false,"advertised":["2.5G","1G","100M"],"linkPartner":[],"active":false},
    ...
    {"port":9,"isSFP":true}]
```
Array of ports EEE configuration and status

| Property | Description
|----------|------------
| `port` | The physical port number
| `isSFP` | Whether the port is SFP
| `advertised` | Links advertised by the port (only for non-SPF links)
| `linkPartner` | Links advertised by the remote-device (only for non-SPF links)
| `active` | Whether EEE is enabled on the port (only for non-SPF links)

## information
```json
{
    "ip_address":"192.168.10.247",
    "ip_gateway":"192.168.10.1",
    "ip_netmask":"255.255.255.0",
    "syslog_server":"0.0.0.0:514",
    "session_timeout":3600,
    "ip_source":"static",
    "mac_address":"01:02:03:04:05:06",
    "hostname":"RTLPlayground-124737",
    "sw_ver":"v0.1.0-af3957e",
    "build_date":"2026-11-05 13:29:46",
    "hw_ver":"Hi-Source HI-k0801WS",
    "pcb_ver": "PCB_K0801W_V2_0",
    "chip_temp":"47.3 C",
    "flash_size":"2 MB",
    "sfp_slot_0":"SOD-10GT-I"
}
```
| Property | Description
|----------|------------
| `ip_address` | Management IP of the switch
| `ip_gateway` | Management IP default gateway
| `ip_netmask` | Management IP netmask
| `syslog_server` | IP and port of syslog server if configured, (`0.0.0.0:514` by default)
| `session_timeout` | In seconds before the HTTP session times out
| `ip_source` | `static` or `dhcp`
| `mac_address` | MAC address of the CPU-port of the switch
| `hostname` | Configured hostname
| `sw_ver` | Firmware build version
| `build_date` | Firmware build date
| `hw_ver` | Hardware description
| `pcb_ver` | The PCB configuration used to build the firmware
| `chip_temp` | Temperature of the chipset
| `flash_size` | Size of the firmware flash
| `sfp_slot_0` | Descriptor of the module in the first SFP slot


## isolation
```json
[
    {"port":1,"allow":[1,2,3,4,5,6,7,8,9,0]},
    {"port":2,"allow":[1,2,3,4,5,6,7,8,9,0]},
    ...
]
```
Array of ports

| Property | Description
|----------|------------
| `port` | The physical port number
| `allow` | The list of physical ports this port is permitted to send to, 0 is the CPU

## l2
```json
[
    {"vlan":"002","mac":"01:02:03:04:05:99","type":"l","port":6,"lag":2,"idx":"0100"},
    {"vlan":"001","mac":"01:02:03:04:05:ef","type":"l","port":6,"lag":2,"idx":"0630"},
    {"vlan":"002","mac":"01:02:03:04:05:ef","type":"l","port":6,"lag":2,"idx":"0a30"},
    {"vlan":"001","mac":"01:02:03:04:05:37","type":"s","port":0,"lag":0,"idx":"0a74"},
    {"vlan":"002","mac":"01:02:03:04:0b:99","type":"l","port":6,"lag":2,"idx":"0100"},
    ...
]
```
List of MAC addresses 

| Property | Description
|----------|------------
| `vlan` | The VLAN ID the mac applies to
| `mac` | The mac address
| `type` | `l` or `s` for `learned` or `static`
| `port` | The physical port the mac applies to
| `lag` | The LAG number it applies to (0 if none)
| `idx` | Internal mac address table index

## lag
```json
{
    "lags":[
        {"lagNum":0,"type":"static","members":[],"hash":"3f"},
        {"lagNum":1,"type":"lacp","members":[6,7],"active_members":[6],"valid":true,"aggregator":"ce11a001a010","hash":"7e"},
        ...
    ],
    "ports":[
        {"p":1,"lag":255,"a":"00","pt":"00","rs":0,"rx":"0000","psys":"000000000000"},
        ...
        {"p":6,"lag":1,"a":"3f","pt":"3f","rs":4,"rx":"0053","psys":"ce11a001a010"},
        {"p":7,"lag":1,"a":"07","pt":"40","rs":0,"rx":"0000","psys":"000000000000"},
        ...
    ]
}
```
Object representing static and LACP lags

| Property | Description
|----------|------------
| `lags` | Array of LAG objects
| `ports` | Array of current port status objects


### LAG objects

| Property | Description
|----------|------------
| `lagNum` | 0-4 the identifier for the LAG
| `type` | `static` or `lacp`
| `members` | The physical port numbers that are configured to be in the LAG
| `active_members` | For LACP LAGs, the members that currently participating
| `valid` | Whether the LACP LAG is currently valid
| `aggregator` | MAC Address of the aggregating device
| `hash` | Load balancing methods to use

### Current port status objects

| Property | Description
|----------|------------
| `p` | Physical port number
| `lag` | Number of the LAG the port is participating in
| `a` | Actor
| `pt` | Partner
| `rs` | RX State
| `rx` | Number of received LACPDUs
| `psys` | Remote system mac address

## lldp
```json
{
    "on":1,
    "portStatus":[
        {"port":1,"name":"","permitted":true},
        ...
    ]
}
```
| Property | Description
|----------|------------
| `on` | Global LLDP on/off, in off the switch will forward LLDPDU's (like a "dumb switch")
| `PortStatus` | Array of LLDP Port Configurations

### LLDP Port Configurations

| Property | Description
|----------|------------
| `port` | Physical port number
| `name` | The name of the port
| `permitted` | if true, the switch transmits LLDPBUs on this port, if false it does not


## mirror
```json
{
    "enabled":false,
    "mirrorPort":1,
    "mirror_rx":[],
    "mirror_tx":[]
}
```
| Property | Description
|----------|------------
| `enabled` | Whether port mirroring is enabled or not
| `mirrorPort` | The port that will receive the mirrored data
| `mirror_rx` | List of physical port numbers that the mirror will receive a copy of received packets for
| `mirror_tx` | List of physical port numbers that the mirror will receive a copy of transmitted packets for


## status
```json
[
    {"port":1,"name":"","isSFP":false,"enabled":true,"adv":["2.5G","1G","100M-FD","100M-HD","10M-HD","10M-FD"],"link":"1000M","mtu":"0x3fff","txG":"0x146","txB":"0x0","rxG":"0x17d","rxB":"0x0","macs":"0x1"},
    {"port":2,"name":"","isSFP":false,"enabled":true,"adv":["2.5G","1G","100M-FD","100M-HD","10M-HD","10M-FD"],"link":"DOWN","mtu":"0x3fff","txG":"0x0","txB":"0x0","rxG":"0x0","rxB":"0x0","macs":"0x0"},
    ...
    {"port":9,"name":"","isSFP":true,"enabled":true,"sfp_options":"0x68","sfp_temp":"0x2640","sfp_vcc":"0x83bd","sfp_txbias":"0x0bb8","sfp_txpower":"0x1390","sfp_rxpower":"0x09c0","sfp_state":"0x02","sfp_vendor":"OEM","sfp_model":"SOD-10GT-I","sfp_serial":"A1A1A1A1A1A1","sfp_los":1,"link":"DOWN","mtu":"0x3fff","txG":"0x0","txB":"0x0","rxG":"0x0","rxB":"0x0","macs":"0x0"}
]
```
| Property | Description
|----------|------------
| `port` | Physical port number
| `name` | User entered name
| `isSFP` | 0/1 is SFP port
| `enabled` | Is port enabled/disabled in the firmware
| `adv` | List of advertised link modes
| `link` | Current link mode
| `mtu` | Maximum Transmission Unit
| `txG` | Number of transmitted good frames
| `txB` | Number of transmitted bad frames
| `rxG` | Number of received good frames
| `rxB` | Number of received bad frames
| `macs` | Number of MAC addresses learned from this port

## stp
```json
{
    "on":true,"type":"rstp","prio":8,"hello":2,"maxage":20,"fwd":15,"txhold":6,"pcs":0,"bh":"flood","tcs":"0x000002a8","rMaxage":20,"rFwd":15,"rootPrio":"8000","rootMac":"010203040506","myMac":"010203040506","cost":"0x00000000","weRoot":true,"tc":"0x0000",
    "ports":[
        {"p":1,"st":3,"role":2,"f":69,"pc":"0x00000000","prio":128,"p2":0,"lk":1,"lg":0,"db":"8000010203040506","dp":"8001","dc":"0x00000000"},
        {"p":2,"st":3,"role":0,"f":69,"pc":"0x00000000","prio":128,"p2":0,"lk":0,"lg":0,"db":"8000010203040506","dp":"8002","dc":"0x00000000"},
        ...
        {"p":102,"st":3,"role":2,"f":69,"pc":"0x00000000","mbr":[6],"prio":128,"p2":0,"lk":1,"lg":0,"db":"8000010203040506","dp":"8066","dc":"0x00000000"}
    ]
}
```
| Property | Description
|----------|------------
| `on` | Enabled/Disabled globally
| `type` | "rstp" (rapid) or "stp" (legacy)
| `prio` | Bridge Priority
| `hello` | Hello time (s)
| `maxage` | Max Age (s)
| `fwd` | This device's configured foward delay (s)
| `txhold` | Transmit Hold
| `pcs` | Path cost ("short" or "long")
| `bh` | BPDU behaviour while disabled, "flood" or "filter"
| `tcs` | Seconds since last topology change
| `rMaxage` | Root bridge max age (s)
| `rFwd` |  Root bridge forward delay (s)
| `rootPrio` | Root bridge priority
| `rootMac` | Root bridge MAC address
| `myMac` | This device MAC address
| `cost` | Cost to reach the root bridge
| `weRoot` | true if we are the root bridge
| `tc` | Number of topology changes
| `ports` | Array of STP Port Status

### STP Port Status
| Property | Description
|----------|------------
| `p` | Physical port number, if > 100 it is LAG number + 100
| `st` | Port state 
| `role` | STP port role
| `f` | Port flags (such as edge, root, BPDUGuard etc)
| `pc` | Path cost
| `mbr` | List of port members (for LAGs)
| `prio` | Port Priority
| `p2` | Point-to-point 
| `lk` | Previous link state that STP saw
| `lg` | Link partner is using legacy STP (not RSTP/MSTP etc)
| `db` | Designated bridge
| `dp` | Designated port
| `dc` | Designated cost

## stpcnt
```json
{
    "on":true,
    "hz":50,
    "tc":"0002",
    "ports":[
        {"p":1,"c":"0000000000009220000000000000000029fc"},
        {"p":2,"c":"00000000000000000000000000000000ffff"},
        {"p":3,"c":"000091790000000300000002000000030005"},
        {"p":4,"c":"000091770000000300000002000000000005"}
        ...
    ]
}
```
| Property | Description
|----------|------------
| `on` | STP enabled
| `hz` | STP timer frequency
| `tc` | Number of topology changes
| `port` | Array of STP port counters


### STP Port Counters
| Property | Description
|----------|------------
| `p` | Physical port number
| `counter` | Hex string of 4x32 bit counters (Number of BPDU RX, Number of BPDU RX, No of TC BPDU's received, No of TC BPDU's sent)


## vlan
```json
{
    "untagged_members":[1,2,3,4,5,8,9],
    "tagged_members":[],
    "name":"",
    "pvid":[1,2,3,4,5,8,9]
}
```
| Property | Description
|----------|------------
| `untagged_members` | Ports that will send participate in this VLAN by transmitting untagged frames
| `tagged_members` | Ports that will send participate in this VLAN by transmitting and receiving tagged frames
| `name` | The name of the VLAN
| `pvid` | List of physical ports that have their Port VLAN ID set to this VLAN

## storm
```json
[
    {
        "port":1,
        "bcast":{"enabled":true,"unit":"kbit/s","rate":"0x000037"},
        "mcast":{"enabled":false,"unit":"pps","rate":"0x000000"},
        "ucast":{"enabled":false,"unit":"pps","rate":"0x000000"},
        "umcast":{"enabled":false,"unit":"pps","rate":"0x000000"}
    }
]
```
Array of port settings
| Property | Description
|----------|------------
| `port` | Physical port number
| `bcast` | Broadcast traffic settings
| `mcast` | Multicast traffic settings
| `ucast` | Unknown unicast traffic settings
| `umcast` | Unknown multicast traffic settings

* Setting
| Property | Description
|----------|------------
| `enabled` | Storm control on or off
| `unit` | The unit of the rate "pps" or "kbit/s"
| `rate` | The value of the limit

## vlanlist
```json
{
    "mgmt":1,"vlan":[
        {"id":1,"name":""},
        {"id":2,"name":"vlanTwo"},
        ...
    ]
}
```
| Property | Description
|----------|------------
| `mgmt` | The management VLAN ID
| `vlan` | Array of VLAN objects

### VLAN Objects

| Property | Description
|----------|------------
| `id` | The VLAN id
| `name` | The VLAN name

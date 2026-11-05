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
 * /vlan.json?vid=<VLANID>
 * /vlanlist.json



## bandwidth
```json
[
    {"port":1,"iLimited":0,"iBW":"0x0fffff","iFC":0,"eLimited":0,"eBW":"0x0fffff"},
    {"port":2,"iLimited":0,"iBW":"0x0fffff","iFC":0,"eLimited":0,"eBW":"0x0fffff"},
    ...
]
```
Array of port properties


| Property | Description
|----------|------------
| `port` | The physical port number
| `iLimited` | 0/1 whether ingress (inbound) limit is applied
| `iBW` | The ingress rate limit applied
| `iFC` | 0/1, 0: packets are dropped, 1: TCP flow-control is used
| `eLimited` | 0/1 whether egress (outbound) limit is applied
| `eBW` | The egress rate limit applied

## counters
GET parameter `port=<PORTNUM>`
Array of counters for the specified port
```json
["0x22cc9","0x51b02","0x2d4","0x1f","0x164","0x262","0x17b","0x0","0x6","0x0","0x0","0x0","0x0","0x0","0x17b","0x0","0x0","0x0","0x0","0x0","0x2b1","0x1f40000002a","0x7800000008","0x1620000004e","0x8400000008","0x5000000a4","0x0","0x0","0x0","0x3dd","0x45700000000","0x3dd","0x45700000000","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x3dd","0x3dd","0x457","0x0","0x0","0x0","0x0","0x0","0x0","0x0"]
```
//TODO this is a mess

## eee
```json
[
    {"port":1,"isSFP":0,"advertised":["2.5G","1G","100M"],"linkPartner":["1G","100M"],"active":1},
    {"port":2,"isSFP":0,"advertised":["2.5G","1G","100M"],"linkPartner":[],"active":0},
    ...
    {"port":9,"isSFP":1}]
```
Array of ports EEE configuration and status

| Property | Description
|----------|------------
| `port` | The physical port number
| `isSFP` | 0/1 whether the port is SFP
| `advertised` | Links advertised by the port (only for non-SPF links)
| `linkPartner` | Links advertised by the remote-device (only for non-SPF links)
| `active` | 0/1 whether EEE is enabled on the port (only for non-SPF links)

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
        {"lagNum":1,"type":"lacp","members":[6,7],"active_members":[6],"valid":1,"aggregator":"ce11a001a010","hash":"7e"},
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
        {"port":1,"name":"","permitted":1},
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
| `permitted` | 0: The switch does not transmit LLDPBUs on this port, 1 it does
| `active_members` | For LACP LAGs, the members that currently participating
| `valid` | Whether the LACP LAG is currently valid
| `aggregator` | MAC Address of the aggregating device
| `hash` | Load balancing methods to use

## mirror
```json
{
    "enabled":0,
    "mirrorPort":1,
    "mirror_rx":[],
    "mirror_tx":[]
}
```
| Property | Description
|----------|------------
| `enabled` | 0/1: Whether port mirroring is enabled or not
| `mirrorPort` | The port that will receive the mirrored data
| `mirror_rx` | List of physical port numbers that the mirror will receive a copy of received packets for
| `mirror_tx` | List of physical port numbers that the mirror will receive a copy of transmitted packets for


## status
```json
[
    {"port":1,"name":"","isSFP":0,"enabled":1,"adv":["2.5G","1G","100M-FD","100M-HD","10M-HD","100M-FD"],"link":"1000M","mtu":"0x3fff","txG":"0x146","txB":"0x0","rxG":"0x17d","rxB":"0x0","macs":"0x1"},
    {"port":2,"name":"","isSFP":0,"enabled":1,"adv":["2.5G","1G","100M-FD","100M-HD","10M-HD","100M-FD"],"link":"DOWN","mtu":"0x3fff","txG":"0x0","txB":"0x0","rxG":"0x0","rxB":"0x0","macs":"0x0"},
    ...
    {"port":9,"name":"","isSFP":1,"enabled":1,"sfp_options":"0x68","sfp_temp":"0x2640","sfp_vcc":"0x83bd","sfp_txbias":"0x0bb8","sfp_txpower":"0x1390","sfp_rxpower":"0x09c0","sfp_state":"0x02","sfp_vendor":"OEM","sfp_model":"SOD-10GT-I","sfp_serial":"A1A1A1A1A1A1","sfp_los":1,"link":"DOWN","mtu":"0x3fff","txG":"0x0","txB":"0x0","rxG":"0x0","rxB":"0x0","macs":"0x0"}
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
    "on":1,"rstp":1,"prio":8,"hello":2,"maxage":20,"fwd":15,"txhold":6,"pcs":0,"bh":0,"tcs":"000002a8","rMaxage":20,"rFwd":15,"rootPrio":"8000","rootMac":"010203040506","myMac":"010203040506","cost":"00000000","weRoot":1,"tc":"0000",
    "ports":[
        {"p":1,"st":3,"role":2,"f":69,"pc":"00000000","prio":128,"p2":0,"lk":1,"lg":0,"db":"8000010203040506","dp":"8001","dc":"00000000"},
        {"p":2,"st":3,"role":0,"f":69,"pc":"00000000","prio":128,"p2":0,"lk":0,"lg":0,"db":"8000010203040506","dp":"8002","dc":"00000000"},
        ...
        {"p":102,"st":3,"role":2,"f":69,"pc":"00000000","mbr":[6],"prio":128,"p2":0,"lk":1,"lg":0,"db":"8000010203040506","dp":"8066","dc":"00000000"}
    ]
}
```
TODO

## vlan
```json
{
    "untagged_members":[1,2,3,4,5,8,9],
    "tagged_members":[],
    "name":"",
    "pvid_array":[1,2,3,4,5,8,9]
}
```
| Property | Description
|----------|------------
| `untagged_members` | Ports that will send participate in this VLAN by transmitting untagged frames
| `tagged_members` | Ports that will send participate in this VLAN by transmitting and receiving tagged frames
| `name` | The name of the VLAN
| `pvid_array` | List of physical ports that have their Port VLAN ID set to this VLAN

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

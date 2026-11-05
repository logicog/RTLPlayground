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

## counters
```json
["0x22cc9","0x51b02","0x2d4","0x1f","0x164","0x262","0x17b","0x0","0x6","0x0","0x0","0x0","0x0","0x0","0x17b","0x0","0x0","0x0","0x0","0x0","0x2b1","0x1f40000002a","0x7800000008","0x1620000004e","0x8400000008","0x5000000a4","0x0","0x0","0x0","0x3dd","0x45700000000","0x3dd","0x45700000000","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x0","0x3dd","0x3dd","0x457","0x0","0x0","0x0","0x0","0x0","0x0","0x0"]
```

## eee
```json
[
    {"port":1,"isSFP":0,"advertised":["2.5G","1G","100M"],"linkPartner":["1G","100M"],"active":1},
    {"port":2,"isSFP":0,"advertised":["2.5G","1G","100M"],"linkPartner":[],"active":0},
    ...
    {"port":9,"isSFP":1}]
```
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

## isolation
```json
[
    {"port":1,"allow":[1,2,3,4,5,6,7,8,9,0]},
    {"port":2,"allow":[1,2,3,4,5,6,7,8,9,0]},
    ...
]
```

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

## mirror
```json
{
    "enabled":0,
    "mirrorPort":1,
    "mirror_rx":[],
    "mirror_tx":[]
}
```

## status
```json
[
    {"port":1,"name":"","isSFP":0,"enabled":1,"adv":["2.5G","1G","100M-FD","100M-HD","10M-HD","100M-FD"],"link":"1000M","mtu":"0x3fff","txG":"0x146","txB":"0x0","rxG":"0x17d","rxB":"0x0","macs":"0x1"},
    {"port":2,"name":"","isSFP":0,"enabled":1,"adv":["2.5G","1G","100M-FD","100M-HD","10M-HD","100M-FD"],"link":"DOWN","mtu":"0x3fff","txG":"0x0","txB":"0x0","rxG":"0x0","rxB":"0x0","macs":"0x0"},
    ...
    {"port":9,"name":"","isSFP":1,"enabled":1,"sfp_options":"0x68","sfp_temp":"0x2640","sfp_vcc":"0x83bd","sfp_txbias":"0x0bb8","sfp_txpower":"0x1390","sfp_rxpower":"0x09c0","sfp_state":"0x02","sfp_vendor":"OEM","sfp_model":"SOD-10GT-I","sfp_serial":"A1A1A1A1A1A1","sfp_los":1,"link":"DOWN","mtu":"0x3fff","txG":"0x0","txB":"0x0","rxG":"0x0","rxB":"0x0","macs":"0x0"}
]
```

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

## vlan
```json
{
    "untagged_members":[1,2,3,4,5,8,9],
    "tagged_members":[],
    "name":"",
    "pvid_array":[1,2,3,4,5,8,9]
}
```

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
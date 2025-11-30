# Trunking

The RTL827x allows to combine multiple ports to a single logical port
(trunking). Up to 2 trunk groups can be defined.

## Trunking control
Two registers RTL837x_TRUNK_CTRL_A (0x4f38) and RTL837x_TRUNK_CTRL_B (0x4f3c)
define the trunk groups. Each holds a bitmap of ports making up the trunk
group.  

## Trunking API
The code currently provides the following functions:
```
void trunk_set(uint8_t group, uint16_t mask) __banked
```

# VLAN configuration on the Serial Console
For testing the following commands are provided on the serial console:
```
trunk <TRUNK-ID> [p1] [p2]...
  create or set a trunk group. Trunk-ID is 1 or 2. ports a physical ports

trunk <VLAN-ID> d
  deletes the trunk group
```

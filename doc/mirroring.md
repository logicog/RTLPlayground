# Mirroring

The RTL827x provides support to mirror packages from a number of ports to a mirroring
ports. For each mirrored port it is possible to define whether received, transmitted
or both types of packages are being mirrored.

## Mirroring control
Mirroring is enabled by setting RTL837x_MIRROR_CTRL(0x6048):

```
RTL837x_MIRROR_CTRL = port << 1 | 0x1
```
Ports are numbered 0-9, 9 being the CPU-port.
Mirroring is stopped by writing 0 to this register.

The mirrored ports are configured in RTL837x_MIRROR_CONF(0x604c):

```
RTL837x_MIRROR_CONF = RRRR TTTT
RRRR: 16 bit mask for ports where received packets are mirrored
TTTT: 16 bit mask for ports where transmitted packets are mirrored
```

## Mirroring API
The code currently provides the following functions:
```
void port_mirror_set(register uint8_t port, __xdata uint16_t rx_pmask, __xdata uint16_t tx_pmask) __banked
void port_mirror_del(void)
```

# Mirroring on the Serial Console
The serial console uses front-panel port numbers:
```
mirror <monitor port> [<source port>[r|t]]...
  Mirror received (r), transmitted (t), or both (no suffix) packet types.

mirror status
  Show the mirroring configuration.

mirror off
  Disable mirroring.
```
Port tokens must contain only a valid front-panel port number and, for source
ports, at most one `r` or `t` suffix. `status` and `off` take no extra arguments.

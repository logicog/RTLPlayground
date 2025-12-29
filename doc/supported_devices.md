# Supported Hardware
The following devices have been tested and are fully working:
- Horaco ZX_SG4T2
- keepLINK kp-9000-6hx-x2 (RTL8372: 4x 2.5GBit + 2x 10GBit SFP+)
- keepLINK KP-9000-6XHML-X2, same as above, but Managed
- keepLINK kp-9000-6hx-x (RTL8372 + RTL8221B 2.5GBit PHY: 5 x 2.5GBit + 1x 10GBit SFP+)
- keepLINK kp-9000-9xh-x-eu (1 x RTL8373 + RTL8224: 8x 2.5GBit + 1x 10GBit SFP+)
- Lianguo LG-SWTGW218AS (RTL8373 + RTL8224 PHY: 8x 2.5GBit + 1x 10GBit SFP+)
- No-Name ZX-SWTGW215AS, managed version of kp-9000-6hx-x, ordered on
  AliExpress as keepLINK 5+1 port managed

Other device based on RTL8272/3 that may work are described here: [Up-N-Atoms 2.5 GBit RTL Switch hacking guide]
(https://github.com/up-n-atom/SWTG118AS)

Many of the RTL8272/3 devices come in versions with PoE support. The RTLPlayground usually also
works on these, however, no support for configuring PoE is provided, simply because these
devices usually just provide PoE on all ports without further configuration possibilitites.

The following forum also discusses this type of switches: [ServeTheHome](https://forums.servethehome.com/index.php?threads/horaco-2-5gbe-managed-switch-8-x-2-5gbe-1-10gb-sfp.41571/)

There are also 16-port unmanaged devices with RTL8272 SoCs, however these devices do not have
serial consoles and use 4 independent RTL8272 SoCs. No central control is provided by RTLPlayground,
even if it has been successfully demonstrated to install RTLPlayground to individual SoCs.
- [GigaPlus GP-S25-1602](https://www.servethehome.com/gigaplus-gp-s25-1602-review-a-cheap-16-port-2-5gbe-and-2-port-10g-switch/)
- [Vimin VM S251602P 16 Port 2.5G PoE Switch With 2x 10G SFP+](https://www.servethehome.com/vimin-vm-s251602p-16-port-2-5g-poe-switch-review-cyperf/vimin-vm-s251602p-16-port-2-5g-poe-switch-with-2x-10g-sfp-battery-2/)

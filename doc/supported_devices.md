# Supported Hardware
The following devices have been tested and are fully working:

| Brand    | Type            | Managed | PCB                                                                                                              | Flash | Chip RTL     |
|----------|-----------------|---------|------------------------------------------------------------------------------------------------------------------|-------|--------------|
| Ampcom   | WAM902-SWTG018AS| No      | [SWTG018AS-A V2.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/SWTG018AS_A_V2_0.md)           |       |              |
| Davuaz   | Da-K6501W       | No      | [PCB-K0501W-V2.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/K0501W_V2_0.md)                 |       |              |
| FOXNEO   | FNS-1200P       | No      | [PCB-K0402W-U13-V2.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/FNS-1200P.md)               | 2M    |  8372        |
| Hisource | Hi-K0402WS      | No      | [PCB-K0402WS-V3.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/PCB-K0402WS-V3.0.md)           |       |              |
| Hisource | Hi-K0801WS      | No      | [PCB-KO801W-V2.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/HI-K0801WS.md)                  |       |              |
| Horaco   | ZX310S-4T2XH    | Yes     | [PCB-SL310S-4T1T1X-V1.0.1-24107](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/ZX310S-4T2XH.md) | 2M    | 8372 + 8261  |
| Horaco   | ZX310S-4T2XT    | Yes     | [PCB-SL310S-4T2XT-V1.0.0-22273](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/ZX310S-4T2XT.md)  | 2M    | 8372 + 8261  |
| Haraco 	 | ZX-SWTG124AS 	 | Yes 	   | [SWTG024AS-v2.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/SWTG024AS.md) 	                  |       | 8272         |
| Horaco   | SWTGW215AS      | Yes     | [SWTG018AS-A V2.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/SWTG018AS_A_V2_0.md)           |       |              |
| keepLINK | KP-9000-9XHML-X | Yes     | [2M-PCB23-V2.2](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/2M-PCB23-V2_2.md)                 | 2M    | 8373 + 8224  |
| keepLINK | KP-9000-9XHML-X | Yes 	   | [2M-PCB23-V3.1](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/2M-PCB23-V3_1.md)                 | 2M 	  | 8273N + 8224N|
| Keeplink | KP-9000-6XH-X2  | No      | [2M-PCB43-V2.1](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/KP-9000-6XH-X2.md)                |       |              |
| LIANGUO  | SWTG024AS 	     | No 	   | [SWTG024AS-v2.0-17452](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/SWTG024AS.md) 	            | 0.5M 	| 8272         |
| Lianguo  | ZX-SWTGW215AS 	 | Yes 	   | [PCB-SWTG115AS-V2.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/SWTGW215AS.md) 	            | 2M    |	8272         |
| Mokerlink| ZX-SWTGW218AS 	 | Yes 	   | [SWTG118AS-V2.0-16029](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/SWTGW218AS.md)             |	2M 	  | 8273N + 8224N|
| Sodola 	 | SL-SWTG124AS-D  | Yes 	   | [SWTG024AS-v2.0-17452](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/SWTG024AS.md) 	            |	2M 	  | 8272         |
| Steamemo | IG204-V1        | No      | [PB-2131](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/STEAMEMO_IG204_V1.md)                   |       |              |
| TrendNet | TEG-S562        | No      | [TEG-S563/EU H/W: V1.0R](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/TEG-S562.md)             | 2M    |              |
| Xikestore| SKS3200M-4GPY2XF| Yes 	   | [SWTG024AS-v1.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/SWTG024AS.md) 	                  | 	    |	8272         |
| XikeStor | SKS3200-8E1X 	 | Yes 	   | [SWTG118AS-V2.1-17462](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/SWTGW218AS.md) 	          | 2M    |              |
| Ztyuav   | Z-QWYT0402      | No      | [PCB-K0402WS-V3.0](https://github.com/logicog/RTLPlayground/blob/main/doc/devices/PCB-K0402WS-V3.0.md)           |       |              |

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

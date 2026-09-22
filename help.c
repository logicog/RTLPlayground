/* `help` command: one usage line per command, grouped by topic. */
#include "machine.h"
#include "rtl837x_common.h"
#include "help.h"

#pragma codeseg BANK3
#pragma constseg BANK3

void help_print(void) __banked
{
	print_string(
		"System:\n"
		"  help                            This list\n"
		"  save [show]                     Persist/show startup config\n"
		"  version                         Firmware version\n"
		"  time                            Tick and second counters\n"
		"  reset                           Reboot the switch\n"
		"  hostname [<name>]               Show or set hostname\n"
		"  passwd <pw>                     Set admin password\n"
		"  history                         Command history\n"
		"Network:\n"
		"  ip [<addr>|dhcp]                Show/set IP or enable DHCP\n"
		"  gw [<addr>]                     Show or set gateway\n"
		"  netmask [<mask>]                Show or set netmask\n"
		"  syslog [on|off|ip <a>|port <p>] Remote syslog\n"
		"  telnet [on|off|bind <ip|any>|timeout <s>] Telnet server\n"
		"  ntp [<server-ip>|off]           SNTP time sync\n"
		"  totp [on|off|secret <b32>]      TOTP 2FA for telnet\n");
	print_string(
		"Ports:\n"
		"  port <p> ...                    Speed/enable/name settings\n"
		"  stat                            Port statistics\n"
		"  sfp                             SFP module info\n"
		"  eee [on|off] [<p>] [<speed>]    Energy-efficient ethernet\n"
		"  mtu <p> <bytes>                 Port MTU\n"
		"  bw in|out <p> <rate>            Bandwidth limits\n"
		"  isolate <p> <off|p>...          Port isolation\n"
		"  mirror <p> <p[t|r]>...|off      Port mirroring\n"
		"  lag <1-4> <ports>|d             Link aggregation\n"
		"  laghash <1-4> <modes>           LAG hash modes\n"
		"Switching:\n"
		"  vlan <id> [name] <p[t]>...      Create/set VLAN, t=tagged\n"
		"  vlan <id> d | show | mgmt       Delete/show/mgmt VLAN\n"
		"  pvid <p> <vid>                  Port PVID\n"
		"  ingress <p[t|u|a]>...           Ingress frame filtering\n"
		"  stp ...                         Spanning tree\n"
		"  igmp on|off|show                IGMP snooping\n"
		"  l2 [forget]                     Show/flush MAC table\n"
		"  mac ...                         Static MAC entries\n"
		"Debug: flash s|j|u, sds, gpio, regget, regset, sdsget,\n"
		"       sdsset, phyget, physet, rnd\n"
		"Telnet session: exit | quit | logout\n");
}

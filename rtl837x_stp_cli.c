/*
 * Command line interface of the Spanning Tree Protocol for the RTL837x platform
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_stp.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata uint8_t cmd_buffer[CMD_BUF_SIZE];
extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[15];
uint8_t cmd_compare(uint8_t start, __code const uint8_t * cmd);
uint8_t atoi_byte(uint8_t idx);
uint8_t cmd_parse_port_separator(uint8_t idx);
extern __xdata uint8_t atoi_results_u8;
extern __xdata char save_cmd;
extern __xdata uint8_t err_status;

#define TIMES_OK(h, m, f)	((uint16_t)2 * ((f) - 1) >= (m) && (m) >= (uint16_t)2 * ((h) + 1))

__xdata uint32_t stp_cli_cost;
__xdata uint8_t  stp_cli_val;


void stp_parse(void) __banked __reentrant
{
	uint8_t port;

	if (cmd_compare(1, "on")) {
		print_string("STP enabled\n");
		stp_enabled = 1;
		stp_setup();
		return;
	}
	if (cmd_compare(1, "off")) {
		print_string("STP disabled\n");
		stp_off();
		stp_enabled = 0;
		return;
	}
	if (cmd_compare(1, "status")) {
		stp_status();
		return;
	}
	if (cmd_compare(1, "clear")) {
		stp_counters_clear();
		return;
	}
	if (cmd_words_len < 3)
		goto err;

	if (cmd_compare(1, "port") || cmd_compare(1, "lag")) {
		if (cmd_words_len < 4)
			goto err;
		stp_lag_map();
		if (cmd_compare(1, "lag")) {
			if (!atoi_byte(cmd_words_b[2]) || !atoi_results_u8 || atoi_results_u8 > STP_LAG_COUNT)
				goto err;
			port = STP_LAG_BASE - 1 + atoi_results_u8;
		} else {
			if (!cmd_parse_port_separator(cmd_words_b[2]))
				goto err;
			port = atoi_results_u8;
			if (stp_ent_of[port] != port) {
				print_string("Error: the port is in lag ");
				write_char('1' + stp_ent_of[port] - STP_LAG_BASE);
				write_char('\n');
				err_status = ERR_INVALID_ARGUMENT;
				return;
			}
		}
		if (cmd_words_len < 5 && !cmd_compare(3, "on") && !cmd_compare(3, "off")
		    && !cmd_compare(3, "mcheck"))
			goto err;
		if (cmd_compare(3, "on")) {
			stp_pflags[port] |= STP_PF_ENABLED;
			stp_pflags[port] &= ~STP_PF_TRIPPED;
			if (stp_enabled) {	/* (re)join: listen first */
				stp_port_admin(port, 1);
			}
		} else if (cmd_compare(3, "mcheck")) {
			if (stp_enabled)
				stp_port_mcheck(port);
		} else if (cmd_compare(3, "off")) {
			stp_pflags[port] &= ~STP_PF_ENABLED;
			if (stp_enabled)
				stp_port_admin(port, 0);	/* plain forwarding */
		} else if (cmd_compare(3, "edge")) {
			/* Also drop the *operational* edge flag: it is what exempts the
			 * port from topology changes and lets it skip the listen period,
			 * so leaving it set would keep the old behaviour until the next
			 * "stp off"/"stp on". An admin edge is operational immediately. */
			stp_pflags[port] &= ~(STP_PF_ADMEDGE | STP_PF_AUTOEDGE | STP_PF_OPEREDGE);
			if (cmd_compare(4, "on"))
				stp_pflags[port] |= STP_PF_ADMEDGE | STP_PF_OPEREDGE;
			else if (cmd_compare(4, "auto"))
				stp_pflags[port] |= STP_PF_AUTOEDGE;
			else if (!cmd_compare(4, "off"))
				goto err;
		} else if (cmd_compare(3, "cost")) {
			/* raw 802.1D value, 0..200000000; 0 = auto (speed-based) */
			stp_cli_cost = 0;
			{
			__xdata uint8_t *cp = &cmd_buffer[cmd_words_b[4]];
			if (*cp < '0' || *cp > '9')
				goto err;
			while (*cp >= '0' && *cp <= '9') {
				stp_cli_cost = stp_cli_cost * 10 + (*cp - '0');
				cp++;
			}
			}
			if (stp_cli_cost > 200000000UL)
				goto err;
			stp_pcost[port] = stp_cli_cost;
		} else if (cmd_compare(3, "p2p")) {
			if (cmd_compare(4, "auto"))
				stp_pp2p[port] = 0;
			else if (cmd_compare(4, "on"))
				stp_pp2p[port] = 1;
			else if (cmd_compare(4, "off"))
				stp_pp2p[port] = 2;
			else
				goto err;
		} else if (cmd_compare(3, "prio")) {
			if (!atoi_byte(cmd_words_b[4]))
				goto err;
			if (atoi_results_u8 > 240 || (atoi_results_u8 & 0x0f))
				goto err;
			stp_pprio[port] = atoi_results_u8;
		} else if (cmd_compare(3, "guard")) {
			stp_pflags[port] &= ~(STP_PF_BPDUGUARD | STP_PF_ROOTGUARD);
			if (cmd_compare(4, "bpdu"))
				stp_pflags[port] |= STP_PF_BPDUGUARD;
			else if (cmd_compare(4, "root"))
				stp_pflags[port] |= STP_PF_ROOTGUARD;
			else if (!cmd_compare(4, "none"))
				goto err;
		} else if (cmd_compare(3, "filter")) {
			if (cmd_compare(4, "on"))
				stp_pflags[port] |= STP_PF_FILTER;
			else if (cmd_compare(4, "off"))
				stp_pflags[port] &= ~STP_PF_FILTER;
			else
				goto err;
		} else {
			goto err;
		}
		return;
	}

	if (!atoi_byte(cmd_words_b[2])) {
		if (cmd_compare(1, "version")) {
			if (cmd_compare(2, "rstp"))
				stp_rstp = 1;
			else if (cmd_compare(2, "stp"))
				stp_rstp = 0;
			else
				goto err;
			return;
		}
		if (cmd_compare(1, "pathcost")) {
			if (cmd_compare(2, "long"))
				stp_pcost_short = 0;
			else if (cmd_compare(2, "short"))
				stp_pcost_short = 1;
			else
				goto err;
			return;
		}
		if (cmd_compare(1, "bpdu")) {
			if (cmd_compare(2, "filter"))
				stp_bpdu_filter = 1;
			else if (cmd_compare(2, "flood"))
				stp_bpdu_filter = 0;
			else
				goto err;
			if (!stp_enabled)
				stp_off();
			return;
		}
		goto err;
	}
	stp_cli_val = atoi_results_u8;

	if (cmd_compare(1, "prio")) {
		if (stp_cli_val > 15)
			goto err;
		stp_prio = stp_cli_val << 4;	/* n * 4096, as the BPDU's high byte */
		stp_prio_apply();
	} else if (cmd_compare(1, "hello")) {
		if (stp_cli_val < 1 || stp_cli_val > 10)
			goto err;
		if (save_cmd && !TIMES_OK(stp_cli_val, stp_maxage_s, stp_fwddelay_s))
			goto times;
		stp_hello_s = stp_cli_val;
	} else if (cmd_compare(1, "maxage")) {
		if (stp_cli_val < 6 || stp_cli_val > 40)
			goto err;
		if (save_cmd && !TIMES_OK(stp_hello_s, stp_cli_val, stp_fwddelay_s))
			goto times;
		stp_maxage_s = stp_cli_val;
	} else if (cmd_compare(1, "fwd")) {
		if (stp_cli_val < 4 || stp_cli_val > 30)
			goto err;
		if (save_cmd && !TIMES_OK(stp_hello_s, stp_maxage_s, stp_cli_val))
			goto times;
		stp_fwddelay_s = stp_cli_val;
	} else if (cmd_compare(1, "txhold")) {
		if (stp_cli_val < 1 || stp_cli_val > 10)
			goto err;
		stp_txhold = stp_cli_val;
	} else {
		goto err;
	}
	return;
times:
	print_string("Error: needs 2*(fwd-1) >= maxage >= 2*(hello+1)\n");
	err_status = ERR_INVALID_ARGUMENT;
	return;
err:
	err_status = ERR_INVALID_ARGUMENT;
	print_string("Error: stp on|off|status|clear | prio <0-15> | hello <1-10> | maxage <6-40> | fwd <4-30> | txhold <1-10> | version rstp|stp | pathcost long|short | bpdu filter|flood | port <1-9>|lag <1-4> on|off|mcheck|edge|cost|prio|guard|filter ...\n");
}

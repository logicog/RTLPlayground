/*
 * "health" command and the health instrumentation hooks (built with HEALTH=1)
 */

#include "cmd_parse_util.h"
#include "cmd_parse_health.h"

#ifdef HEALTH

#include <8051.h>
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"
#include "uip/uip.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern volatile __xdata uint32_t ticks;
extern volatile __xdata uint8_t sfr_data[4];
extern __xdata uint16_t management_vlan;

#include "health_idle.h"
/*
 * Health instrumentation. The counters live in xdata and are fed from the
 * main loop through the banked entry points below; the "health" command
 * prints everything in one go. All figures are raw hex, diffing two dumps
 * gives the rates. Phase times are in system ticks (SYS_TICK_HZ = 200,
 * 5 ms each); a phase counts as slow when it spans 2 or more ticks.
 */
extern __xdata uint16_t len_left;	/* httpd: bytes still to send of the current file */
extern __xdata uint8_t entry;		/* httpd: index of the file being sent */

static __xdata uint32_t health_loops;
__xdata uint16_t health_rx_frames;

static __xdata uint16_t ph_last_t;
static __xdata uint8_t  ph_max[HEALTH_PHASES];
static __xdata uint16_t ph_slow[HEALTH_PHASES];

static __xdata uint16_t idle_win_tick;
static __xdata uint8_t  busy_last;
static __xdata uint8_t  busy_max;

/* The ISR increments ticks between the two byte reads of a 16-bit load, so
 * read until two loads agree. */
static uint16_t ticks_lo(void)
{
	uint16_t a, b;
	a = (uint16_t)ticks;
	do {
		b = a;
		a = (uint16_t)ticks;
	} while (a != b);
	return a;
}


static void health_stamp(void)
{
	__xdata uint8_t h, l;
	do {
		health_now_tick = ticks_lo();
		do {
			h = TH2;
			l = TL2;
		} while (h != TH2);
	} while (health_now_tick != ticks_lo());
	health_now_t2 = ((uint16_t)h << 8) | l;
}


void health_loop_start(void) __banked
{
	health_loops++;
	ph_last_t = ticks_lo();
	if (!health_t2_per_tick) {
		health_t2_per_tick = 0 - RCAP2_U16;
		idle_win_tick = ph_last_t;
	}
	health_window = ph_last_t - idle_win_tick;
	if (health_window < SYS_TICK_HZ)
		return;
	busy_last = health_busy_pct();
	if (busy_last > busy_max)
		busy_max = busy_last;
	health_idle_ticks = 0;
	health_idle_sub = 0;
	idle_win_tick = ph_last_t;
}


void health_sleep_start(void) __banked
{
	health_stamp();
	health_sleep_tick = health_now_tick;
	health_sleep_t2 = health_now_t2;
}


void health_sleep_end(void) __banked
{
	health_stamp();
	health_idle_add();
}


void health_phase(uint8_t id) __banked
{
	__xdata uint16_t now = ticks_lo();
	__xdata uint16_t d = now - ph_last_t;
	ph_last_t = now;
	if (d > 0xff)
		d = 0xff;
	if ((uint8_t)d > ph_max[id])
		ph_max[id] = d;
	if (d >= 2)
		ph_slow[id]++;
}


/* Fill the stack area above the current frame with a pattern; the dump scans
 * for how much of it survived. The 8051 stack grows upward, so everything
 * above SP is free at this point. Interrupts are held off so an ISR does not
 * push into the area mid-paint. */
void health_stack_paint(void) __banked
{
	__xdata uint8_t from = SP + 4;
	__xdata uint8_t ea = EA;
	EA = 0;
	for (uint8_t i = 0xff; i >= from; i--)
		*(__idata uint8_t *)i = 0xa5;
	EA = ea;
}


static void ph_line(__code const char *name, uint8_t id)
{
	print_string(name);
	print_string(" max ");
	print_byte(ph_max[id]);
	print_string(" slow ");
	print_short(ph_slow[id]);
	write_char('\n');
}


void health_show(void) __banked
{
	__xdata uint8_t i;

	print_string("up ");
	reg_read_m(RTL837X_REG_SEC_COUNTER);
	print_byte(sfr_data[0]); print_byte(sfr_data[1]);
	print_byte(sfr_data[2]); print_byte(sfr_data[3]);
	print_string(" ticks ");
	print_long(ticks);
	print_string(" loops ");
	print_long(health_loops);
	print_string(" rx ");
	print_short(health_rx_frames);
	write_char('\n');

	/* Phase timing: max duration seen (ticks of 5 ms) and slow-pass count */
	ph_line("link", HEALTH_PH_LINK);
	ph_line("sfp ", HEALTH_PH_SFP);
	ph_line("rx  ", HEALTH_PH_RX);
	ph_line("tx  ", HEALTH_PH_TX);
	ph_line("stp ", HEALTH_PH_STP);
	ph_line("cmd ", HEALTH_PH_CMD);
	ph_line("lldp", HEALTH_PH_LLDP);

	print_string("cpu busy ");
	itoa(busy_last);
	print_string("% max ");
	itoa(busy_max);
	print_string("%\n");

	/* Stack: current depth plus how much of the painted area is untouched */
	print_string("sp ");
	print_byte(SP);
	print_string(" untouched ");
	i = 0;
	while (*(__idata uint8_t *)(0xff - i) == 0xa5 && i < 0x86)
		i++;
	print_byte(i);
	write_char('\n');

	/* The TCP connection slot. One on this configuration; a build with more
	 * has to say which slot each line is, so make that a visible decision. */
#if UIP_CONNS != 1
#error "health: the tcp dump prints a single slot, extend it for UIP_CONNS > 1"
#endif
	i = 0;
	{
		print_string("tcp ");
		print_string("st ");
		print_byte(uip_conns[i].tcpstateflags);
		print_string(" tmr ");
		print_byte(uip_conns[i].timer);
		print_string(" rtx ");
		print_byte(uip_conns[i].nrtx);
		print_string(" rto ");
		print_byte(uip_conns[i].rto);
		print_string(" len ");
		print_short(uip_conns[i].len);
		print_string(" mss ");
		print_short(uip_conns[i].mss);
		write_char('\n');
		print_string("  lport ");
		print_short(HTONS(uip_conns[i].lport));
		print_string(" rport ");
		print_short(HTONS(uip_conns[i].rport));
		print_string(" rip ");
		print_short(HTONS(uip_conns[i].ripaddr[0]));
		write_char(' ');
		print_short(HTONS(uip_conns[i].ripaddr[1]));
		write_char('\n');
	}

	/* httpd transfer state and the protocol flags */
	print_string("httpd left ");
	print_short(len_left);
	print_string(" entry ");
	print_byte(entry);
	print_string(" stp ");
	print_byte(stp_enabled);
	print_string(" mvlan ");
	print_short(management_vlan);
	write_char('\n');
}

void cmd_parse_health(void)
{
	health_show();
}

#endif /* HEALTH */

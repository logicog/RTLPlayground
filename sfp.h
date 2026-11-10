#ifndef SFP_H
#define SFP_H

#define SFP_WAKE_TICKS	100
#define SFP_DOM_TICKS	SYS_TICK_HZ
#define SFP_IO_TICKS	(SYS_TICK_HZ / 10)
#define SFP_RETRY_TICKS	(2 * SYS_TICK_HZ)
#define SFP_TRIES	5

#define SFP_ABSENT	0
#define SFP_WAKE	1
#define SFP_READ	2
#define SFP_FAULT	3
#define SFP_UP		4

extern __xdata uint8_t sfp_pins_last;
extern __xdata char sfp_module_vendor[2][17];
extern __xdata char sfp_module_model[2][17];
extern __xdata char sfp_module_serial[2][17];
extern __xdata uint8_t sfp_options[2];
extern __xdata uint8_t sfp_speed[2];
extern __xdata uint8_t sfp_quirks[2];
extern __xdata uint8_t sfp_id[2][2];
extern __xdata uint8_t sfp_cal[2][36];
extern __xdata uint8_t sfp_dom[2][16];
extern __xdata uint8_t sfp_ready;
extern __xdata uint8_t sfp_state[2];

void setup_sfp_gpio(void) __banked;
void handle_sfp(void) __banked;
void sfp_task(void) __banked;

#endif

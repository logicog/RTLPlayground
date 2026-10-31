#ifndef SFP_H
#define SFP_H

#define SFP_WAKE_TICKS	100
#define SFP_DOM_TICKS	SYS_TICK_HZ
#define SFP_IO_TICKS	(SYS_TICK_HZ / 10)

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

void setup_sfp_gpio(void) __banked;
void handle_sfp(void) __banked;
void sfp_task(void) __banked;

#endif

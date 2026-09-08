#ifndef SFP_H
#define SFP_H

extern __xdata uint8_t sfp_pins_last;
extern __xdata char sfp_module_vendor[2][17];
extern __xdata char sfp_module_model[2][17];
extern __xdata char sfp_module_serial[2][17];
extern __xdata uint8_t sfp_options[2];
extern __xdata uint8_t sfp_speed[2];
extern __xdata uint8_t sfp_quirks[2];

bool sfp_print_info(uint8_t sfp) __banked;
bool sfp_read_field(__xdata char *dst, uint8_t sfp, uint8_t start, uint8_t length) __banked __reentrant;
bool sfp_get_info(uint8_t sfp) __banked;
void sfp_apply_quirks(uint8_t sfp) __banked __reentrant;
void setup_sfp_gpio(void) __banked;
void handle_sfp(void) __banked;

#endif

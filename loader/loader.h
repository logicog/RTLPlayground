#ifndef LOADER_LOADER_H
#define LOADER_LOADER_H

#include <stdint.h>
#include <stdbool.h>

#define TICKS_1S	159

void put(uint8_t c);
void write_char(char c);
void print_string(__code const char *p);
void print_byte(uint8_t a);
void reg_read(uint16_t a);
void reg_write(uint16_t a);
void timer0_start(void);
bool tick(void);

extern __xdata char line[128];
extern uint8_t lp;

extern __xdata bool net_used;

void net(void);
void ipaddr(void);
void serverip(void);
void tftp(void);
void up_begin(void);
bool up_add(__xdata const uint8_t *d, uint16_t n);
void up_end(void);

#endif

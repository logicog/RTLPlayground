#ifndef _RTL837X_LEDS_H_
#define _RTL837X_LEDS_H_

#define LEDS_2G5		0x00001
#define LEDS_TWO_PAIR_1G	0x00002
#define LEDS_1G			0x00004
#define LEDS_500M		0x00008
#define LEDS_100M		0x00010
#define LEDS_10M		0x00020
#define LEDS_LINK		0x00040
#define LEDS_LINK_FLASH		0x00080
#define LEDS_ACT		0x00100
#define LEDS_RX			0x00200
#define LEDS_TX			0x00400
#define LEDS_COL		0x00800
#define LEDS_DUPLEX		0x01000
#define LEDS_TRAINING		0x02000
#define LEDS_MASTER		0x04000
#define LEDS_10G		0x10000
#define LEDS_TWO_PAIR_5G	0x20000
#define LEDS_5G			0x40000
#define LEDS_TWO_PAIR_2G5	0x80000

#include <stdint.h>
void leds_dump(void) __banked;
void leds_setup(void) __banked;

#endif

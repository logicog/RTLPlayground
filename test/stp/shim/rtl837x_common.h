#ifndef _SHIM_RTL837X_COMMON_H_
#define _SHIM_RTL837X_COMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

struct rtl_tag {
	uint16_t tag;
	uint8_t version;
	uint8_t reason;
	uint16_t flags;
	uint16_t pmask;		/* TX: port bitmask; RX: 4-bit port number */
};

struct vlan_tag {
	uint16_t svlan;
	uint16_t vlan;
};

#define RTL_FRAME_TAG_ID	0x8899
#define RTL_FRAME_TAG_VERSION	0x04
#define RTL_FRAME_DESC_SIZE	12
#define RTL_TAG_LEARN_DIS	0x0020

#define PMASK_9		0x1ff
#define PMASK_6		0x1f8
#define PMASK_CPU	0x200

#define CMD_BUF_SIZE	128
#define CPU_PORT	9

#define ERR_OK			0
#define ERR_INVALID_ARGUMENT	3

#define HTONS(n) ((uint16_t)((((uint16_t)(n)) << 8) | (((uint16_t)(n)) >> 8)))

extern bool stp_enabled;
void print_string(char *s);
void print_byte(uint8_t b);
void print_short(uint16_t v);
void print_long(uint32_t v);
void write_char(char c);
void itoa(uint8_t v);
void itoa_short(uint16_t v);
void print_string_x(char *s);
void print_reg(uint16_t reg);
void reg_read_m(uint16_t addr);
void reg_write_m(uint16_t addr);
void tcpip_output(void);

#endif

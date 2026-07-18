/*
 * Host-build shim for rtl837x_common.h - just enough for rtl837x_lacp.c.
 * Layouts of rtl_tag/vlan_tag and the frame-desc size mirror the real header;
 * the harness static-asserts the resulting on-wire struct sizes.
 */
#ifndef _SHIM_RTL837X_COMMON_H_
#define _SHIM_RTL837X_COMMON_H_

#include <stdint.h>
#include <string.h>	/* memcpy/memset - firmware provides its own */

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

#define RTL_TAG_SIZE		(sizeof (struct rtl_tag))
#define VLAN_TAG_SIZE		(sizeof (struct vlan_tag))
#define RTL_FRAME_TAG_ID	0x8899
#define RTL_FRAME_TAG_VERSION	0x04
#define RTL_FRAME_DESC_SIZE	12
#define ETHER_HEADER_SIZE	14

#define HTONS(n) ((uint16_t)((((uint16_t)(n)) << 8) | (((uint16_t)(n)) >> 8)))

/* Console output - provided by the harness */
void print_string(char *s);
void print_byte(uint8_t b);
void print_short(uint16_t v);
void write_char(char c);

#endif

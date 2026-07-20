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

/* Host-build shim: port masks used by lacp_isolation_update() */
#ifndef PMASK_CPU
#define CPU_PORT   9
#define PMASK_9    0x1ff
#define PMASK_6    0x1f8
#define PMASK_CPU  0x200
#endif

/* Host-build shim: directed-egress/strip-tag bit (bit15 of TX pmask word) */
#ifndef RTL_TAG_ALLOW
#define RTL_TAG_ALLOW 0x8000
#endif

/* Host-build shim: tag flags word bits (tag_rtl8_4) */
#ifndef RTL_TAG_LEARN_DIS
#define RTL_TAG_LEARN_DIS 0x0020
#define RTL_TAG_KEEP      0x0080
#endif

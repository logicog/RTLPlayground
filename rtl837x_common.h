#ifndef _RTL837X_STDIO_H_
#define _RTL837X_STDIO_H_

#include "uip/uip-conf.h"

// Headers for calls in the common code area (HOME/BANK0)

#define SBUF_SIZE 256

// For RX data, a propriatary RTL FRAME is inserted. Instead of 0x0800 for IPv4,
// the RTL_FRAME_TAG_ID is used as part of an 8-byte tag. When VLAN is activated,
// the VLAN tag is inserted after the RTL tag
// See here for the RTL tag: https://github.com/torvalds/linux/commit/1521d5adfc2b557e15f97283c8b7ad688c3ebc40
#define RTL_TAG_SIZE		8
#define VLAN_TAG_SIZE		4
#define RTL_FRAME_TAG_ID	0x8899

// For RX and TX, an 8 byte header describing the frame to be moved to the Asic
// and received from the Asic is used
#define RTL_FRAME_HEADER_SIZE	8

// This is the standard size of an Ethernet frame header
#define ETHER_HEADER_SIZE	14


/**
 * Representation of a 48-bit Ethernet address.
 */
struct uip_eth_addr {
   uint8_t addr[6];
};

extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_SIZE+2];

void print_string(__code char *p);
void print_long(__xdata uint32_t a);
void print_short(uint16_t a);
void print_byte(uint8_t a);
void print_sfr_data(void);
void print_phy_data(void);
void phy_write(uint16_t phy_mask, uint8_t dev_id, uint16_t reg, uint16_t v);
void phy_read(uint8_t phy_id, uint8_t dev_id, uint16_t reg);
void reg_read(uint16_t reg_addr);
void reg_read_m(uint16_t reg_addr);
void reg_write(uint16_t reg_addr);
void reg_write_m(uint16_t reg_addr);
void delay(uint16_t t);
void sleep(uint16_t t);
void write_char(char c);
void print_reg(uint16_t reg);
uint8_t sfp_read_reg(uint8_t slot, uint8_t reg);
void reg_bit_set(uint16_t reg_addr, char bit);
void reg_bit_clear(uint16_t reg_addr, char bit);
void reset_chip(void);
void memcpy(__xdata void * __xdata dst, __xdata const void * __xdata src, uint16_t len);
void memcpyc(register __xdata uint8_t *dst, register __code uint8_t *src, register uint16_t len);
void memset(register __xdata uint8_t *dst, register __xdata uint8_t v, register uint8_t len);
uint16_t strlen(register __code const char *s);
uint16_t strlen_x(register __xdata const char *s);
void strtox(register __xdata uint8_t *dst, register __code const char *s);
void tcpip_output(void);
void print_string_x(__xdata char *p);

#endif

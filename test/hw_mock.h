/*
 * hw_mock.h - a simulated RTL837x register file and table engine.
 *
 * The firmware reaches the ASIC only through reg_read/reg_read_m/reg_write/
 * reg_write_m and the SFR_DATA_* registers, so mocking that edge is enough to
 * run the table code on the host. Every register is a 32-bit word in a flat
 * file; a few addresses are live: writing TBL_CTRL with EXECUTE performs the
 * table operation at once and clears the bit, STAT_GET fetches a counter,
 * L2_TBL_FLUSH_CTRL drops entries. Nothing here ever reports busy, so a
 * polling loop in the firmware runs exactly once.
 *
 * The VLAN and L2 entry layouts mirror what rtl837x_port.c writes and what
 * httpd/page_impl.c reads; the decoders below are an independent statement
 * of that layout, so a test can compare the firmware's two ends against it.
 */
#ifndef TEST_HW_MOCK_H
#define TEST_HW_MOCK_H

#include <stdint.h>
#include <stdbool.h>

void     hw_reset(void);                       /* every register 0, tables empty */
void     hw_reg_set(uint16_t addr, uint32_t v);
uint32_t hw_reg_get(uint16_t addr);

extern uint8_t       sfr_data[4];               /* the firmware's register scratch, owned here */
extern unsigned long hw_reads;                 /* reg_read + reg_read_m calls since hw_reset */
extern unsigned long hw_writes;                /* reg_write + reg_write_m calls since hw_reset */

/* VLAN table: the 32-bit word the firmware wrote for that VID, 0 = never written */
uint32_t hw_vlan_word(uint16_t vid);

/* L2 table: entries as the three data words the firmware exchanges with the
 * engine. Word B: bit 29 valid, bits 31:30 the low two bits of the port (or
 * of the member mask), bits 27:16 VID, bits 15:0 the first two MAC octets.
 * Word A: the last four MAC octets. Word C: bit 16 static, bits 1:0 the high
 * two bits of the port; for a multicast MAC bits 7:0 are the member mask
 * shifted right by two. */
#define HW_L2_MAX 64
struct hw_l2 {
	bool     present;
	uint16_t idx;
	uint32_t a, b, c;   /* DATA_IN_A/B/C on write, L2_DATA_OUT_A/B/C on read */
};

/* Pointers returned below stay valid until the next table operation. */
int             hw_l2_count(void);
const struct hw_l2 *hw_l2_at(int n);           /* n-th present entry in idx order */
const struct hw_l2 *hw_l2_find(const uint8_t mac[6], uint16_t vid);
/* Put an entry the way the switch would hold it: a learned unicast entry on
 * one port, or a static one. mc entries use pmask instead of a port. */
void hw_l2_put(uint16_t idx, const uint8_t mac[6], uint16_t vid, uint16_t port_or_pmask, bool is_static);
void hw_l2_decode(const struct hw_l2 *e, uint8_t mac[6], uint16_t *vid, uint16_t *port_or_pmask,
		  bool *valid, bool *is_static, bool *mc);

/* MIB counter the next STAT_GET for (port, counter) returns */
void hw_counter_set(uint8_t port, uint8_t counter, uint64_t value);

/* I2C master: writing I2C_CTRL with bit 0 set runs the transfer at once on a
 * device added here, a 256-byte register file at a bus (as I2CBUS() encodes
 * it) and 7-bit address. A transfer to a missing device sets the error bit,
 * as a NAK would. Transfers are logged for inspection. */
uint8_t *hw_i2c_dev(uint8_t bus, uint8_t addr);	/* adds the device on first use */
struct hw_i2c_xfer {
	uint32_t ctrl;		/* the I2C_CTRL word that started it */
	uint8_t  reg, len;
	bool     write;
	uint8_t  data[16];
};
int                       hw_i2c_count(void);
const struct hw_i2c_xfer *hw_i2c_at(int n);

#endif /* TEST_HW_MOCK_H */

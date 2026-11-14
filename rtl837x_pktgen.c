#pragma codeseg BANK3
#pragma constseg BANK3

#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "cmd_parser.h"
#include "rtl837x_pktgen.h"

extern __xdata uint8_t sfr_data[4];

#define PKTGEN_REG(g, n) (RTL837X_PKTGEN0_CTRL0 + (uint16_t)(g) * PKTGEN_INSTANCE_STRIDE + (n) * 4)
#define PKTGEN_DBG(g) (RTL837X_PKTGEN0_CTRL15 + (uint16_t)(g) * 4)
#define PKTGEN_SRAM_WAIT 1000

static __xdata uint8_t pktgen_mac_buf[6];
static __xdata uint32_t pktgen_v;
static __xdata uint32_t pktgen_glb;
static __xdata uint32_t pktgen_fifo;
static __xdata uint32_t pktgen_low;
static __xdata uint16_t pktgen_high;

static uint32_t pktgen_read(uint16_t reg) __reentrant
{
    reg_read_m(reg);
    return ((uint32_t)sfr_data[0] << 24) | ((uint32_t)sfr_data[1] << 16) | ((uint16_t)sfr_data[2] << 8) | sfr_data[3];
}

static void pktgen_field(uint16_t reg, uint8_t shift, uint16_t mask, uint16_t v) __reentrant
{
    pktgen_v = (pktgen_read(reg) & ~((uint32_t)mask << shift)) | ((uint32_t)(v & mask) << shift);
    REG_SET(reg, pktgen_v);
}

static void pktgen_label(__code const char *s, uint32_t v) __reentrant
{
    print_string(s);
    if (v >> 16)
        print_long(v);
    else
        itoa_short(v);
}

static void pktgen_print_mac(uint16_t high) __reentrant
{
    pktgen_mac_buf[0] = high >> 8;
    pktgen_mac_buf[1] = high;
    pktgen_mac_buf[2] = pktgen_low >> 24;
    pktgen_mac_buf[3] = pktgen_low >> 16;
    pktgen_mac_buf[4] = pktgen_low >> 8;
    pktgen_mac_buf[5] = pktgen_low;
    print_mac(pktgen_mac_buf);
}

void pktgen_show(void) __banked __reentrant
{
    uint8_t g;

    pktgen_glb = pktgen_read(RTL837X_PKTGEN_GLOBAL_CTRL);
    pktgen_fifo = pktgen_read(RTL837X_PKTGEN_G2XG_FIFO_CTRL);
    print_string("global ");
    print_long(pktgen_glb);
    print_string("  fifo ");
    print_long(pktgen_fifo);
    write_char('\n');
    for (g = 0; g < PKTGEN_COUNT; g++) {
        print_string("gen ");
        write_char('0' + g);
        print_string(": sel ");
        write_char('0' + ((pktgen_glb >> (PKTGEN_GLOBAL_SEL_SHIFT + 2 * g)) & 3));
        print_string((pktgen_fifo >> (PKTGEN_FIFO_CLK_SHIFT + g)) & 1 ? " xg" : " ge");
        print_string((pktgen_glb >> g) & 1 ? " running" : " stopped");
        print_string((pktgen_glb >> (PKTGEN_GLOBAL_DONE_SHIFT + g)) & 1 ? " done" : "");
        pktgen_label("\n  burst ", pktgen_read(PKTGEN_REG(g, 1)));
        pktgen_label(" x ", pktgen_read(PKTGEN_REG(g, 2)));
        pktgen_label(" gap ", pktgen_read(PKTGEN_REG(g, 0)));
        pktgen_label(" sent ", pktgen_read(PKTGEN_DBG(g)));
        pktgen_v = pktgen_read(PKTGEN_REG(g, 3));
        pktgen_label("\n  len ", (pktgen_v >> PKTGEN_LEN_START_SHIFT) & PKTGEN_LEN_MASK);
        pktgen_label("-", pktgen_v & PKTGEN_LEN_MASK);
        pktgen_label(" mode ", (pktgen_v >> PKTGEN_LEN_MODE_SHIFT) & 3);
        print_string((pktgen_v >> PKTGEN_ETHERTYPE_MOD_BIT) & 1 ? " type on " : " type off ");
        pktgen_v = pktgen_read(PKTGEN_REG(g, 4));
        print_short(pktgen_v >> 16);
        pktgen_label(" payload mode ", (pktgen_v >> PKTGEN_PAYLOAD_MOD_BIT) & 1);
        pktgen_label(" mask ", pktgen_v & PKTGEN_PAYLOAD_MASK);
        print_string("\n  dmac ");
        pktgen_v = pktgen_read(PKTGEN_REG(g, 6));
        pktgen_low = pktgen_read(PKTGEN_REG(g, 5));
        pktgen_print_mac(pktgen_v >> 16);
        print_string(" smac ");
        pktgen_low = pktgen_read(PKTGEN_REG(g, 7));
        pktgen_print_mac(pktgen_v);
        write_char('\n');
    }
}

void pktgen_sel(uint8_t g, uint8_t sel) __banked __reentrant
{
    pktgen_field(RTL837X_PKTGEN_GLOBAL_CTRL, PKTGEN_GLOBAL_SEL_SHIFT + 2 * g, 3, sel);
}

void pktgen_clock(uint8_t g, bool xg) __banked __reentrant
{
    pktgen_field(RTL837X_PKTGEN_G2XG_FIFO_CTRL, PKTGEN_FIFO_CLK_SHIFT + g, 1, xg);
}

void pktgen_burst(uint8_t g, uint16_t count, uint16_t size) __banked __reentrant
{
    REG_SET(PKTGEN_REG(g, 1), count);
    REG_SET(PKTGEN_REG(g, 2), size);
}

void pktgen_gap(uint8_t g, uint16_t len) __banked __reentrant
{
    REG_SET(PKTGEN_REG(g, 0), len);
}

void pktgen_len(uint8_t g, uint16_t start, uint16_t end, uint8_t mode) __banked __reentrant
{
    pktgen_v = pktgen_read(PKTGEN_REG(g, 3)) & ((uint32_t)1 << PKTGEN_ETHERTYPE_MOD_BIT);
    pktgen_v |= (uint32_t)(mode & 3) << PKTGEN_LEN_MODE_SHIFT;
    pktgen_v |= (uint32_t)(start & PKTGEN_LEN_MASK) << PKTGEN_LEN_START_SHIFT;
    pktgen_v |= end & PKTGEN_LEN_MASK;
    REG_SET(PKTGEN_REG(g, 3), pktgen_v);
}

void pktgen_type(uint8_t g, uint16_t ethertype, bool on) __banked __reentrant
{
    pktgen_field(PKTGEN_REG(g, 4), 16, 0xffff, ethertype);
    pktgen_field(PKTGEN_REG(g, 3), PKTGEN_ETHERTYPE_MOD_BIT, 1, on);
}

void pktgen_payload_mode(uint8_t g, uint8_t mode, uint8_t mask) __banked __reentrant
{
    pktgen_field(PKTGEN_REG(g, 4), 0, 0xf, ((mode & 1) << PKTGEN_PAYLOAD_MOD_BIT) | (mask & PKTGEN_PAYLOAD_MASK));
}

void pktgen_mac(uint8_t g, bool dst, __xdata const uint8_t *mac) __banked __reentrant
{
    pktgen_low = ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) | ((uint16_t)mac[4] << 8) | mac[5];
    pktgen_high = ((uint16_t)mac[0] << 8) | mac[1];
    if (dst) {
        REG_SET(PKTGEN_REG(g, 5), pktgen_low);
        pktgen_field(PKTGEN_REG(g, 6), 16, 0xffff, pktgen_high);
    } else {
        REG_SET(PKTGEN_REG(g, 7), pktgen_low);
        pktgen_field(PKTGEN_REG(g, 6), 0, 0xffff, pktgen_high);
    }
}

static void pktgen_payload_access(uint8_t addr, uint8_t data, bool write) __reentrant
{
    pktgen_v = ((uint32_t)write << PKTGEN_SRAM_WRITE_BIT) | ((uint32_t)addr << PKTGEN_SRAM_ADDR_SHIFT) | ((uint32_t)1 << PKTGEN_SRAM_TRIGGER_BIT) | data;
    uint16_t wait = PKTGEN_SRAM_WAIT;

    REG_SET(RTL837X_PKTGEN_PAYLOAD_IND_ACCESS_CTRL, pktgen_v);
    do {
        reg_read_m(RTL837X_PKTGEN_PAYLOAD_IND_ACCESS_CTRL);
    } while ((sfr_data[2] & 0x80) && --wait);
    if (!wait)
        print_string("pattern memory busy\n");
}

void pktgen_payload_write(uint8_t addr, __xdata const uint8_t *data, uint8_t len) __banked __reentrant
{
    while (len--)
        pktgen_payload_access(addr++, *data++, true);
}

void pktgen_payload_read(uint8_t addr) __banked __reentrant
{
    uint8_t i;

    print_byte(addr);
    print_string(": ");
    for (i = 0; i < 16; i++) {
        pktgen_payload_access(addr + i, 0, false);
        print_byte(sfr_data[3]);
        write_char(' ');
    }
    write_char('\n');
}

void pktgen_run(uint8_t g, bool on) __banked __reentrant
{
    pktgen_field(RTL837X_PKTGEN_GLOBAL_CTRL, g, 1, on);
}

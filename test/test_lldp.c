#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_lldp.h"
#include "machine.h"
#include "uip.h"
#include "rtl837x_trap.h"
#include <json-c/json.h>

uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
uint16_t uip_len;
uint8_t outbuf[TCP_OUTBUF_SIZE];
uint16_t slen;
volatile uint32_t ticks;
/* Hardware-only edges used by integration's mismatch serializer. */
uint16_t SFR_DATA_U16;
void reg_read_m(uint16_t reg) { SFR_DATA_U16 = 0; }
uint8_t port_lag_of(uint8_t port) { return 0xff; }
const struct machine machine = { .min_port = 0, .max_port = 7,
    .log_to_phys_port = { 1, 2, 3, 4, 5, 6, 7, 8 } };
static char console[4096];
static size_t console_len;

void memcpyc(uint8_t *dst, const uint8_t *src, uint16_t len) { memcpy(dst, src, len); }
uint16_t strtox(uint8_t *dst, const char *src)
{
    size_t len = strlen(src);
    memcpy(dst, src, len);
    return len;
}
void write_char(char c)
{
    assert(console_len + 1 < sizeof(console));
    console[console_len++] = c;
    console[console_len] = 0;
}
void print_string(const char *s) { while (*s) write_char(*s++); }

static const uint8_t chassis_mac[] = { 4, 0x02, 0, 0x22, 0, 0x44, 0x55 };
static const uint8_t port_text[] = { 7, 'p' };

static void frame_start(bool tagged)
{
    memset(uip_buf, 0xa5, sizeof(uip_buf));
    memcpy(uip_buf, "\x01\x80\xc2\0\0\x0e", 6);
    memset(uip_buf + 12, 0, RTL_TAG_SIZE);
    if (tagged) {
        memcpy(uip_buf + 20, "\x81\0\0\1", 4);
        uip_len = 24;
    } else {
        uip_len = 20;
    }
    uip_buf[uip_len++] = 0x88;
    uip_buf[uip_len++] = 0xcc;
}

static void tlv(uint8_t type, const uint8_t *bytes, uint16_t len)
{
    assert(uip_len + len + 2 < sizeof(uip_buf));
    uip_buf[uip_len++] = (type << 1) | (len >> 8);
    uip_buf[uip_len++] = len;
    if (len) memcpy(uip_buf + uip_len, bytes, len);
    uip_len += len;
}

static void ids(const uint8_t *chassis, uint16_t chassis_len,
                const uint8_t *port, uint16_t port_len)
{
    static const uint8_t ttl[] = { 0, 120 };
    tlv(1, chassis, chassis_len);
    tlv(2, port, port_len);
    tlv(3, ttl, sizeof(ttl));
}

static unsigned neighbor_count(void)
{
    unsigned count = 0;
    for (unsigned i = 0; i < LLDP_NB_MAX; i++) count += !!lldp_nb[i].ttl;
    return count;
}

static struct json_object *output(void)
{
    struct json_object *root;
    slen = 1;
    outbuf[0] = '{';
    /* The production serializer appends a property to an existing object. */
    slen += strtox(outbuf + slen, "\"enabled\":true");
    lldp_neighbors_to_html();
    assert(slen + 2 < sizeof(outbuf));
    outbuf[slen++] = '}';
    outbuf[slen] = 0;
    root = json_tokener_parse((char *)outbuf);
    assert(root);
    return root;
}

static void expect_field(const char *name, const char *expected)
{
    struct json_object *root = output(), *neighbors, *value;
    assert(json_object_object_get_ex(root, "neighbors", &neighbors));
    assert(json_object_array_length(neighbors) == 1);
    assert(json_object_object_get_ex(json_object_array_get_idx(neighbors, 0), name, &value));
    if (strcmp(json_object_get_string(value), expected)) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", name, expected, json_object_get_string(value));
        assert(0);
    }
    json_object_put(root);
}

static void receive(void)
{
    tlv(0, NULL, 0);
    assert(lldp_in());
}

static void test_binary_port_mac(void)
{
    static const uint8_t port[] = { 3, 0, 0x11, 0, 0xab, 0, 0xff };
    for (unsigned tagged = 0; tagged < 2; tagged++) {
        lldp_init();
        frame_start(tagged);
        ids(chassis_mac, sizeof(chassis_mac), port, sizeof(port));
        receive();
        expect_field("portId", "00:11:00:ab:00:ff");
        expect_field("chassis", "02:00:22:00:44:55");
        console_len = 0;
        lldp_show();
        assert(strstr(console, "00:11:00:ab:00:ff"));
    }
}

static void test_text_and_refresh(void)
{
    static const uint8_t port[] = { 7, 'A', '"', '\\', 0, 1, 0x7f, 0xff, 'Z' };
    static const uint8_t text[] = { 'N', '"', '\\', 0, 2, 0x7f, 0xff, 'Z' };
    lldp_init();
    frame_start(false);
    ids(chassis_mac, sizeof(chassis_mac), port, sizeof(port));
    tlv(5, text, sizeof(text));
#ifdef LLDP_NB_SYS_DESC_LENGTH
    tlv(6, text, sizeof(text));
    tlv(4, text, sizeof(text));
#endif
    receive();
    expect_field("portId", "A......Z");
    expect_field("sysName", "N......Z");
#ifdef LLDP_NB_SYS_DESC_LENGTH
    expect_field("sysDesc", "N......Z");
    expect_field("portDesc", "N......Z");
#endif
    frame_start(false);
    ids(chassis_mac, sizeof(chassis_mac), port_text, sizeof(port_text));
    tlv(5, (const uint8_t *)"X", 1);
    receive();
    assert(neighbor_count() == 1);
    expect_field("portId", "p");
    expect_field("sysName", "X");
#ifdef LLDP_NB_SYS_DESC_LENGTH
    expect_field("sysDesc", "");
    expect_field("portDesc", "");
#endif
}

static void test_bounded_ids(void)
{
    uint8_t port[257];
    memset(port, 'x', sizeof(port));
    port[0] = 7;
    for (unsigned len = 18; len <= 256; len += len == 18 ? 1 : 237) {
        lldp_init();
        frame_start(false);
        ids(chassis_mac, sizeof(chassis_mac), port, len);
        receive();
        expect_field("portId", "xxxxxxxxxxxxxxxxx");
    }
#ifdef LLDP_NB_CHASSIS_LENGTH
    static const uint8_t textual_mac[] = { 7, '0','0',':','1','1',':','2','2',':','3','3',':','4','4',':','5','5' };
    lldp_init();
    frame_start(false);
    ids(textual_mac, sizeof(textual_mac), port_text, sizeof(port_text));
    receive();
    expect_field("chassis", "00:11:22:33:44:55");
    for (unsigned len = 18; len <= 256; len += len == 18 ? 1 : 237) {
        lldp_init();
        frame_start(false);
        ids(port, len, port_text, sizeof(port_text));
        receive();
        expect_field("chassis", "xxxxxxxxxxxxxxxxx");
        assert(lldp_nb[0].chassis_len == 17);
    }
    static const uint8_t raw[] = { 7, 'A', '"', '\\', 0, 1, 0x7f, 0xff, 'Z' };
    lldp_init();
    frame_start(false);
    ids(raw, sizeof(raw), port_text, sizeof(port_text));
    receive();
    expect_field("chassis", "A......Z");
#endif
}

static void test_invalid_ids(void)
{
    uint8_t bad[257];
    static const struct { uint16_t len; uint8_t subtype; } cases[] = {
        { 0, 7 }, { 1, 7 }, { 2, 0 }, { 2, 8 }, { 257, 7 },
        { 2, 3 }, { 6, 3 }, { 8, 3 }
    };
    memset(bad, 'x', sizeof(bad));
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        bad[0] = cases[i].subtype;
        lldp_init();
        frame_start(false);
        ids(chassis_mac, sizeof(chassis_mac), bad, cases[i].len);
        receive();
        assert(neighbor_count() == 0);
    }
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        bad[0] = cases[i].subtype == 3 ? 4 : cases[i].subtype;
        lldp_init();
        frame_start(false);
        ids(bad, cases[i].len, port_text, sizeof(port_text));
        receive();
        assert(neighbor_count() == 0);
    }
    lldp_init();
    frame_start(false);
    tlv(1, chassis_mac, sizeof(chassis_mac));
    receive();
    assert(neighbor_count() == 0);
}

static void test_short_frames(void)
{
    for (unsigned tagged = 0; tagged < 2; tagged++) {
        frame_start(tagged);
        ids(chassis_mac, sizeof(chassis_mac), port_text, sizeof(port_text));
        tlv(5, (const uint8_t *)"name", 4);
        tlv(0, NULL, 0);
        uint16_t full = uip_len;
        for (unsigned len = 0; len < full; len++) {
            lldp_init();
            uip_len = len;
            lldp_in();
            /* Mandatory TTL/order/End validation is outside this bounded-ID parser's scope. */
            unsigned offset = tagged ? 26u : 22u;
            if (len != offset + 13 && len != offset + 17 && len != offset + 23)
                assert(neighbor_count() == 0);
        }
    }
}

int main(void)
{
    test_binary_port_mac();
    test_text_and_refresh();
    test_bounded_ids();
    test_invalid_ids();
    test_short_frames();
    puts("LLDP parser/output tests passed");
    return 0;
}

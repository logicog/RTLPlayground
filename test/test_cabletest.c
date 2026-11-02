#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_phy.h"
#include "rtl837x_cabletest.h"
#include "rtl837x_sfr.h"
#include "phy.h"
#include "machine.h"

volatile uint16_t SFR_DATA_U16;
volatile uint32_t ticks;
uint8_t err_status;
struct phy_settings phy_settings;
const struct machine machine = {.is_sfp={0,0,0,0,0,0,0,0,1}};
struct machine_runtime machine_detected;
static uint16_t bmcr, power, address, raw_length, ctrl;
static uint8_t ready;
void print_string(const char *s) {(void)s;}
void write_char(char c) {(void)c;}
void print_phys_port(uint8_t p) {(void)p;}
void itoa_short(uint16_t v) {(void)v;}
void phy_write(uint8_t p,uint8_t dev,uint16_t reg,uint16_t val) {
    (void)p;(void)dev;
    if(reg==PHY_MMD31_FEDCR)bmcr=val;
    if(reg==0xa436)address=val;
    if(reg==0xa422)ctrl=val;
}
void phy_read(uint8_t p,uint8_t dev,uint16_t reg) {
    (void)p;(void)dev;SFR_DATA_U16=0;
    if(reg==PHY_MMD31_FEDCR)SFR_DATA_U16=bmcr;
    if(reg==0xa610)SFR_DATA_U16=power;
    if(reg==0xa422)SFR_DATA_U16=ready?0x8000:0;
    if(reg==0xa438)SFR_DATA_U16=((address-0x8026)%4==0)?0x48:raw_length;
}
static void advance(unsigned t) {ticks+=t;cabletest_tick();}
int main(void) {
    assert(cabletest_supported(4)&&cabletest_supported(7)&&!cabletest_supported(3)&&!cabletest_supported(8));
    phy_settings.port=4;bmcr=0x1140;power=0;raw_length=236;
    cabletest_run();assert(ct_state&&bmcr==0x0940&&!ct_have);
    phy_settings.port=5;err_status=0;cabletest_run();assert(err_status&&ct_port==4);
    advance(SYS_TICK_HZ);assert(ctrl==0xf1&&bmcr==0x0140);
    ready=1;advance(SYS_TICK_HZ/20);
    assert(!ct_state&&ct_have&&ct_valid&&bmcr==(0x1140|0x0200)&&ctrl==0);
    assert(ct_result[0]==CABLE_RESULT_OPEN&&ct_len[0]==10);
    raw_length=65535;rtl_phy_cable_test_get_status(4);assert(ct_len[0]==7602);
    phy_settings.port=4;ready=0;bmcr=0x1140;cabletest_run();advance(SYS_TICK_HZ);
    for(unsigned n=0;n<100;n++)advance(SYS_TICK_HZ/20);
    assert(!ct_state&&ct_have&&!ct_valid&&bmcr==(0x1140|0x0200));
    power=0x0800;err_status=0;cabletest_run();assert(err_status&&!ct_state);
    power=0;phy_settings.port=8;err_status=0;cabletest_run();assert(err_status&&!ct_state);
    puts("Cable diagnostics: support, busy/off guards, pair distance, overflow, timeout and PHY restoration PASS");
}

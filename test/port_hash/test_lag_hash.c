/* Host test for port_lag_members_set(): the default load-balancing hash must be
 * seeded into the CURRENT trunk's register (BASE + lag*4), never trunk 0's, and
 * must not overwrite a hash that is already set. Compiles the REAL function
 * (extracted verbatim from rtl837x_port.c) against register-recording shims. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define __banked
#define __xdata
#define __code

/* register defs (mirror rtl837x_regs.h) */
#define RTL837X_TRK_MBR_CTRL_BASE  0x4f38
#define RTL837X_TRK_HASH_CTRL_BASE 0x4f48
#define LAG_HASH_L2_SMAC 0x02
#define LAG_HASH_L2_DMAC 0x04
#define LAG_HASH_L3_SIP  0x08
#define LAG_HASH_L3_DIP  0x10
#define LAG_HASH_L4_SPORT 0x20
#define LAG_HASH_L4_DPORT 0x40
#define LAG_HASH_DEFAULT (LAG_HASH_L2_SMAC|LAG_HASH_L2_DMAC|LAG_HASH_L3_SIP|LAG_HASH_L3_DIP|LAG_HASH_L4_SPORT|LAG_HASH_L4_DPORT)

uint8_t SFR_DATA_24, SFR_DATA_16, SFR_DATA_8, SFR_DATA_0;
uint8_t sfr_data[4];
static uint32_t reg_store[0x10000];

void reg_read_m(uint16_t r){ uint32_t v=reg_store[r];
  sfr_data[0]=(v>>24)&0xff; sfr_data[1]=(v>>16)&0xff; sfr_data[2]=(v>>8)&0xff; sfr_data[3]=v&0xff; }
void reg_write(uint16_t r){ reg_store[r]=((uint32_t)SFR_DATA_24<<24)|((uint32_t)SFR_DATA_16<<16)|((uint32_t)SFR_DATA_8<<8)|SFR_DATA_0; }
void print_string(char*s){(void)s;} void print_byte(uint8_t b){(void)b;}
void print_short(uint16_t s){(void)s;} void write_char(char c){(void)c;}

#define REG_SET(r,v) do{ SFR_DATA_24=((uint32_t)(v)>>24)&0xff; SFR_DATA_16=((uint32_t)(v)>>16)&0xff; \
  SFR_DATA_8=((uint32_t)(v)>>8)&0xff; SFR_DATA_0=(v)&0xff; reg_write(r); }while(0)
#define REG_WRITE(r,a,b,c,d) do{ SFR_DATA_24=(a); SFR_DATA_16=(b); SFR_DATA_8=(c); SFR_DATA_0=(d); reg_write(r); }while(0)

/* THE REAL FUNCTION (extracted from rtl837x_port.c at build time) */
#include "port_lag_members_set.inc"

static int fails;
#define CK(cond,msg,...) do{ if(cond){printf("PASS  " msg "\n",##__VA_ARGS__);} else {printf("FAIL  " msg "\n",##__VA_ARGS__); fails++;} }while(0)

int main(void){
  for(int lag=0; lag<4; lag++){
    memset(reg_store,0,sizeof(reg_store));
    port_lag_members_set(lag, 0x0180);
    uint16_t h = RTL837X_TRK_HASH_CTRL_BASE + (lag<<2);
    uint16_t m = RTL837X_TRK_MBR_CTRL_BASE  + (lag<<2);
    CK(reg_store[h]==LAG_HASH_DEFAULT, "lag%d: default 0x%02x -> hash reg 0x%04x", lag, LAG_HASH_DEFAULT, h);
    CK(reg_store[m]==0x0180,           "lag%d: members 0x0180 -> mbr reg 0x%04x", lag, m);
    if(lag) CK(reg_store[RTL837X_TRK_HASH_CTRL_BASE]==0, "lag%d: trunk-0 hash NOT clobbered", lag);
  }
  /* guard: existing hash must survive a later member change */
  memset(reg_store,0,sizeof(reg_store));
  reg_store[RTL837X_TRK_HASH_CTRL_BASE+(1<<2)] = 0x06;   /* lag1 already L2 */
  port_lag_members_set(1, 0x00c0);
  CK(reg_store[RTL837X_TRK_HASH_CTRL_BASE+(1<<2)]==0x06, "guard: preset lag1 hash 0x06 preserved");
  printf(fails? "\n=== FAIL (%d) ===\n" : "\n=== ALL PASS ===\n", fails);
  return fails?1:0;
}

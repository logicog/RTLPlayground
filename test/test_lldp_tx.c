/* Check emitted LLDP bytes; only the NIC and register edges are mocked. */
#include <assert.h>
#include <stdio.h>
#include "rtl837x_common.h"
#include "rtl837x_lldp.h"
#include "machine.h"
#include "uip.h"
#include "rtl837x_port.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"

uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
uint16_t uip_len;
struct uip_eth_addr uip_ethaddr = {{2, 0, 0, 0, 0, 7}};
uip_ipaddr_t uip_hostaddr;
const struct machine machine = {.machine_name="Host LLDP test", .min_port=0, .max_port=1,
    .log_to_phys_port={1,9}};
char hostname[24]="12345678901234567890123";
char port_names[9][PORT_NAME_SIZE];
bool lldp_enabled;
uint8_t sfr_data[4];
volatile uint16_t SFR_DATA_U16;
extern uint16_t lldp_logical_port_status;
extern uint8_t lldp_tlv_mask;
static uint8_t frames[2][1600];
static uint16_t lengths[2];
static unsigned emitted;

void memcpyc(uint8_t *d,const uint8_t *s,uint16_t n){memcpy(d,s,n);}
uint16_t strlen_x(const char *s){return strlen(s);}
void reg_read_m(uint16_t r){(void)r; SFR_DATA_U16=1522; memset(sfr_data,0,4);}
void phy_read(uint8_t p,uint8_t d,uint16_t r){(void)p;(void)d;(void)r;SFR_DATA_U16=0;}
uint16_t port_pvid_get(uint8_t p){return p+1;}
uint8_t port_lag_of(uint8_t p){(void)p;return PORT_LAG_NONE;}
void tcpip_output(void){
    const uint8_t *f=uip_buf+RTL_FRAME_DESC_SIZE;
    unsigned mask=((unsigned)f[18]<<8)|f[19], p=mask==1?0:1;
    assert(mask==1||mask==2);
    assert(uip_len<=sizeof frames[p]);
    memcpy(frames[p],f,uip_len);lengths[p]=uip_len;emitted++;
}
static const uint8_t *tlv(unsigned p,unsigned wanted,unsigned *length){
    unsigned pos=22;
    while(pos+2<=lengths[p]){
        unsigned type=frames[p][pos]>>1;
        unsigned n=((frames[p][pos]&1)<<8)|frames[p][pos+1];
        assert(pos+2+n<=lengths[p]);
        if(type==wanted){*length=n;return frames[p]+pos+2;}
        if(!type)break;
        pos+=2+n;
    }
    return NULL;
}
static void check(unsigned p,unsigned ttl,int management){
    unsigned n;
    const uint8_t *v=tlv(p,1,&n);assert(v&&n==7&&v[0]==4&&!memcmp(v+1,uip_ethaddr.addr,6));
    v=tlv(p,2,&n);assert(v&&n==7&&v[0]==5&&!memcmp(v+1,p?"Port 9":"Port 1",6));
    v=tlv(p,3,&n);assert(v&&n==2&&(((unsigned)v[0]<<8)|v[1])==ttl);
    v=tlv(p,5,&n);assert(v&&n==23&&!memcmp(v,hostname,n));
    v=tlv(p,7,&n);assert(v&&n==4&&!memcmp(v,"\0\4\0\4",4));
    v=tlv(p,8,&n);
    if(management){assert(v&&n==12&&v[0]==5&&v[1]==1&&!memcmp(v+2,uip_hostaddr,4));assert(!memcmp(v+6,"\1\0\0\0\0\0",6));}
    else assert(!v);
    v=tlv(p,0,&n);assert(v&&n==0);
}
int main(void){
    uint8_t ip[4]={10,0,0,7};memcpy(uip_hostaddr,ip,4);
    lldp_tlv_mask=LLDP_TX_ALL;lldp_logical_port_status=3;
    lldp_send();assert(emitted==2);check(0,120,1);check(1,120,1);
    puts("PASS normal TX: Port 1/Port 9 subtype 5, TTL 120, bridge caps 0x0004/0x0004, IPv4 10.0.0.7, End TLV");
    emitted=0;lldp_shutdown(2);assert(emitted==1);check(1,0,1);
    puts("PASS selected-port shutdown: only Port 9, TTL 0");
    emitted=0;lldp_shutdown(lldp_logical_port_status);assert(emitted==2);check(0,0,1);check(1,0,1);
    puts("PASS all-enabled-port shutdown: both ports, TTL 0");
    memset(uip_hostaddr,0,sizeof uip_hostaddr);emitted=0;lldp_send();assert(emitted==2);check(0,120,0);check(1,120,0);
    puts("PASS unconfigured IPv4: no Management Address TLV");
    lldp_tlv_mask&=~LLDP_TX_SYSCAP;lldp_send();unsigned n;assert(!tlv(0,7,&n));
    puts("PASS optional System Capabilities switch omits TLV");
    return 0;
}

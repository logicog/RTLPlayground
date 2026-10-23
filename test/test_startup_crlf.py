#!/usr/bin/env python3
"""Compile configuration replay and MTU parsing with mocked flash/register edges."""
import pathlib, subprocess
root=pathlib.Path(__file__).resolve().parent.parent
src=(root/'cmd_parser.c').read_text()
def function(signature):
    start=src.index(signature)
    end=src.index('{',start)+1
    depth=1
    while depth:
        depth+=(src[end]=='{')-(src[end]=='}')
        end+=1
    return src[start:end]+'\n'
body='\n'.join(line for line in src.splitlines() if line.startswith('#define DEFAULT_MTU '))
body+='\n'+''.join(function(signature) for signature in (
    'uint8_t cmd_compare(', 'uint8_t atoi_short(', 'uint8_t cmd_parse_port(',
    'uint8_t cmd_parse_port_separator(', 'void parse_mtu(void)',
    'void execute_config(void) __banked'))
out=root/'test/build';out.mkdir(exist_ok=True)
harness=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#define __xdata
#define __banked
#define __code
#include "machine.h"
#include "rtl837x_regs.h"
#define SFR_DATA_24 sfr_data[0]
#define SFR_DATA_16 sfr_data[1]
#define SFR_DATA_8 sfr_data[2]
#define SFR_DATA_0 sfr_data[3]
#define SFR_DATA_U16 ((uint16_t)sfr_data[2]<<8|sfr_data[3])
#define CONFIG_START 0
#define CONFIG_LEN 1024
#define FLASH_READ_BURST_SIZE 256
#define CMD_BUF_SIZE 64
#define ERR_OK 0
#define NUL 0
#define PASSWORD "test-default"
uint8_t flash_buf[256],cmd_buffer[64],save_cmd,err_status;
uint8_t sfr_data[4],cmd_words_len,cmd_words_b[15],atoi_results_u8;
uint16_t atoi_results_short;
uint32_t rx_limits[10];
const struct machine machine={.phys_to_log_port={
    MACHINE_MIN_PORT,MACHINE_MIN_PORT+1,MACHINE_MIN_PORT+2,
    MACHINE_MIN_PORT+3,MACHINE_MIN_PORT+4,MACHINE_MIN_PORT+5,
    MACHINE_MIN_PORT+6,MACHINE_MIN_PORT+7,MACHINE_MIN_PORT+8}};
void parse_mtu(void);
unsigned rx_port(uint16_t address){
    assert(address>=0x1250&&((address-0x1250)&255)==0);
    unsigned port=(address-0x1250)>>8;assert(port<10);return port;
}
void reg_read_m(uint16_t address){
    uint32_t value=rx_limits[rx_port(address)];
    for(unsigned i=0;i<4;i++)sfr_data[i]=value>>(24-8*i);
}
void reg_write(uint16_t address){
    rx_limits[rx_port(address)]=(uint32_t)sfr_data[0]<<24|
        (uint32_t)sfr_data[1]<<16|(uint16_t)sfr_data[2]<<8|sfr_data[3];
}
char passwd[32],image[1024],seen[16][64];int count;
struct {uint32_t addr;uint16_t len;} flash_region;
void strtox(char*d,const char*s){strcpy(d,s);}
void flash_read_bulk(uint8_t*d){memcpy(d,image+flash_region.addr,256);}
void print_string(const char*s){(void)s;}
void print_string_x(uint8_t*s){(void)s;}
void write_char(char c){(void)c;}
void print_byte(uint8_t n){(void)n;}
void print_short(uint16_t n){(void)n;}
void print_phys_port(uint8_t n){(void)n;}
void cmd_error(const char*s){(void)s;err_status=2;}
void clear_command_history(void){}
void cmd_tokenize(void){
    cmd_words_len=0;
    for(unsigned i=0;cmd_buffer[i];i++)
        if(cmd_buffer[i]!=' '&&(!i||cmd_buffer[i-1]==' '))
            cmd_words_b[cmd_words_len++]=i;
    err_status=cmd_buffer[0]=='#'?1:0;
}
void cmd_parser(void){
    assert(count<16);strcpy(seen[count++],(char*)cmd_buffer);
    if(!strncmp((char*)cmd_buffer,"mtu ",4))parse_mtu();
}
"""
tests=r"""
void run(const char*s){
    memset(image,0,sizeof image);strncpy(image,s,sizeof image-1);count=0;
    for(unsigned p=0;p<10;p++)rx_limits[p]=0xbfffffff;
    execute_config();assert(save_cmd==1);
}
int main(void){
run("one\ntwo\n");assert(count==2&&!strcmp(seen[1],"two"));
run("one\rtwo\r");assert(count==2&&!strcmp(seen[0],"one"));
run("one\r\ntwo\r\n");assert(count==2&&!strcmp(seen[1],"two"));
run("\r\n# ignored\r\none\r\n\r\ntwo");assert(count==2&&!strcmp(seen[0],"one"));
run("one\0ignored");assert(count==1&&!strcmp(seen[0],"one"));
char longline[300];memset(longline,'x',sizeof longline);strcpy(longline+280,"\r\nok\r\n");run(longline);assert(count==1&&!strcmp(seen[0],"ok"));
/* CR at the end of a flash burst, LF at the start of the next. */
char boundary[300];memset(boundary,'\n',255);strcpy(boundary+255,"\r\nok\r\n");run(boundary);assert(count==1&&!strcmp(seen[0],"ok"));
run("");
for(unsigned p=0;p<10;p++)
    assert(rx_limits[p]==(MACHINE_PORT_VALID(p)?0xb17c85f2u:0xbfffffffu));
run("mtu 2 9000\n");
for(unsigned p=0;p<10;p++)
    assert(rx_limits[p]==(p==MACHINE_MIN_PORT+1?0x08ca2328u:
        MACHINE_PORT_VALID(p)?0xb17c85f2u:0xbfffffffu));
puts("startup replay, 1522-byte defaults, port bounds and saved MTU override passed");return 0;
}
"""
(out/'startup_crlf.c').write_text(harness+body+tests)
for board in ('SWTGW218AS','KP_9000_6XHML_X2'):
    binary=out/('test_startup_crlf_'+board)
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-pointer-sign',
        '-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
        '-I'+str(root),'-DMACHINE_NAME='+board,str(out/'startup_crlf.c'),
        '-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)

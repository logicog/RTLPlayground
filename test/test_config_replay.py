#!/usr/bin/env python3
"""Compile the actual generic startup reader without SNMP or physical Flash."""
from pathlib import Path
import os,subprocess,shlex
root=Path(__file__).resolve().parent.parent
src=(root/'cmd_parser.c').read_text()
def function(name):
 a=src.index(name);b=src.index('{',a);i=b+1;depth=1
 while depth:
  depth+=(src[i]=='{')-(src[i]=='}');i+=1
 return src[a:i]+'\n'
code=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define __xdata
#define __code
#define __banked
#define NUL 0
#define CMD_BUF_SIZE 128
#define N_WORDS 15
#define ERR_OK 0
#define ERR_CMD_TOO_LONG 2
#define ERR_TOO_MANY_ARGUMENTS 3
#define CONFIG_START 0
#define CONFIG_LEN 512
#define FLASH_READ_BURST_SIZE 256
#define PASSWORD "test-default"
uint8_t cmd_buffer[128],cmd_words_len,cmd_words_b[15],err_status,save_cmd,flash_buf[256];
char passwd[21],image[512],seen[8][128];unsigned calls;
struct {uint32_t addr;uint16_t len;} flash_region;
void strtox(char *d,const char *s){strcpy(d,s);}
void flash_read_bulk(uint8_t *d){assert(flash_region.addr+256<=sizeof image);memcpy(d,image+flash_region.addr,256);}
void print_string(const char *s){(void)s;}
void print_string_x(uint8_t *s){(void)s;}
void write_char(char c){(void)c;}
void clear_command_history(void){}
void cmd_parser(void){assert(calls<8);strcpy(seen[calls++],(char*)cmd_buffer);}
'''
code+=function('void cmd_tokenize(')+function('void execute_config(')+r'''
static void boot(const char *text){memset(image,0,sizeof image);strcpy(image,text);calls=0;execute_config();assert(save_cmd==1&&!strcmp(passwd,PASSWORD));}
int main(void){
 boot("hostname lab\r\nip 192.0.2.1\r\n");assert(calls==2&&!strcmp(seen[0],"hostname lab")&&!strcmp(seen[1],"ip 192.0.2.1"));
 boot("hostname lab\rip 192.0.2.1\r");assert(calls==2);
 boot("hostname lab\nip 192.0.2.1");assert(calls==2);
 char cross[400];memset(cross,'\n',248);strcpy(cross+248,"foo bar\r\nnext\r\n");boot(cross);assert(calls==2&&!strcmp(seen[0],"foo bar")&&!strcmp(seen[1],"next"));
 memset(cross,'x',150);strcpy(cross+150,"\r\nnext\r\n");boot(cross);assert(calls==1&&!strcmp(seen[0],"next"));
 boot("");assert(!calls);
 puts("PASS generic startup replay without SNMP: CRLF/CR/LF, burst-boundary delimiters, final line, overlong recovery and empty config");return 0;}
'''
out=root/'test/build/config-replay';out.mkdir(parents=True,exist_ok=True);c=out/'replay.c';c.write_text(code)
cc=shlex.split(os.environ.get('CC','cc'));exe=out/('test-'+Path(cc[-1]).name)
subprocess.run(cc+['-std=c11','-Wall','-Wextra','-Werror','-Wno-pointer-sign','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(c),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)

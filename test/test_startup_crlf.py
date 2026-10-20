#!/usr/bin/env python3
"""Compile the current firmware execute_config() with mocked flash/tokenizer edges."""
import pathlib, subprocess
root=pathlib.Path(__file__).resolve().parent.parent
src=(root/'cmd_parser.c').read_text()
start=src.index('void execute_config(void) __banked')
end=src.index('// Execute multiple commands',start)
body=src[start:end]
out=root/'test/build';out.mkdir(exist_ok=True)
harness=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define __xdata
#define __banked
#define CONFIG_START 0
#define CONFIG_LEN 1024
#define FLASH_READ_BURST_SIZE 256
#define CMD_BUF_SIZE 64
#define ERR_OK 0
#define NUL 0
#define PASSWORD "test-default"
uint8_t flash_buf[256],cmd_buffer[64],save_cmd,err_status;
char passwd[32],image[1024],seen[16][64];int count;
struct {uint32_t addr;uint16_t len;} flash_region;
void strtox(char*d,const char*s){strcpy(d,s);}
void flash_read_bulk(uint8_t*d){memcpy(d,image+flash_region.addr,256);}
void print_string(const char*s){(void)s;}
void print_string_x(uint8_t*s){(void)s;}
void write_char(char c){(void)c;}
void clear_command_history(void){}
void cmd_tokenize(void){err_status=cmd_buffer[0]=='#'?1:0;}
void cmd_parser(void){assert(count<16);strcpy(seen[count++],(char*)cmd_buffer);}
"""
tests=r"""
void run(const char*s){memset(image,0,sizeof image);strncpy(image,s,sizeof image-1);count=0;execute_config();assert(save_cmd==1);}
int main(void){
run("one\ntwo\n");assert(count==2&&!strcmp(seen[1],"two"));
run("one\rtwo\r");assert(count==2&&!strcmp(seen[0],"one"));
run("one\r\ntwo\r\n");assert(count==2&&!strcmp(seen[1],"two"));
run("\r\n# ignored\r\none\r\n\r\ntwo");assert(count==2&&!strcmp(seen[0],"one"));
run("one\0ignored");assert(count==1&&!strcmp(seen[0],"one"));
char longline[300];memset(longline,'x',sizeof longline);strcpy(longline+280,"\r\nok\r\n");run(longline);assert(count==1&&!strcmp(seen[0],"ok"));
/* CR at the end of a flash burst, LF at the start of the next. */
char boundary[300];memset(boundary,'\n',255);strcpy(boundary+255,"\r\nok\r\n");run(boundary);assert(count==1&&!strcmp(seen[0],"ok"));
puts("startup CR/LF/CRLF, comments, NUL, overlong and burst-boundary cases passed");return 0;
}
"""
(out/'startup_crlf.c').write_text(harness+body+tests)
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',str(out/'startup_crlf.c'),'-o',str(out/'test_startup_crlf')],check=True)
subprocess.run([str(out/'test_startup_crlf')],check=True)

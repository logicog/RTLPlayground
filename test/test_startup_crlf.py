#!/usr/bin/env python3
"""Run firmware startup replay and password parsing against mocked flash."""
import pathlib, subprocess, sys
root=pathlib.Path(__file__).resolve().parent.parent
src=(root/'cmd_parser.c').read_text()
start=src.index('void execute_config(void) __banked')
end=src.index('// Execute multiple commands',start)
body=src[start:end]
words_start=src.index('#define N_WORDS ')
words=src[words_start:src.index('\n\n', words_start)]
tokenize=src[src.index('void cmd_tokenize(void) __banked'):src.index('// Print GPIO status')]
passwd_handler=src[src.index('void parse_passwd(void)'):src.index('void parse_eee(void)')]
harness=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "httpd/request_buffer.h"
#include "rtl837x_common.h"
""" + words + r"""
#define FLASH_READ_BURST_SIZE 256
uint8_t flash_buf[FLASH_READ_BURST_SIZE],cmd_buffer[CMD_BUF_SIZE],save_cmd,err_status;
char image[CONFIG_LEN],seen[16][CMD_BUF_SIZE];int count;
struct {uint32_t addr;uint16_t len;} flash_region;
union http_request_buffer request_buf;
uint16_t strtox(uint8_t*d,const char*s){strcpy((char *)d,s);return strlen(s);}
void flash_read_bulk(uint8_t*d){memcpy(d,image+(flash_region.addr-CONFIG_START),FLASH_READ_BURST_SIZE);}
void print_string(const char*s){(void)s;}
void print_string_x(char*s){(void)s;}
void write_char(char c){(void)c;}
void clear_command_history(void){}
void cmd_error(const char *msg){(void)msg;err_status=1;}
void session_init(void){}
bool strstart(const uint8_t *line, const uint8_t *prefix)
{
	return strncmp((const char *)line, (const char *)prefix, strlen((const char *)prefix)) == 0;
}
void blake3_digest(struct blake3_work *work, uint8_t len)
{
	static const char input[] = "\0xyztest";
	assert(len == sizeof(input) - 1 && !memcmp(work->block, input, len));
	memcpy(work->block, "0123456789abcdef", 16);
}
void parse_passwd(void);
void cmd_parser(void)
{
	if (cmd_buffer[0] == '#') {
		return;
	}
	assert(count < 16);
	strcpy(seen[count++], (char *)cmd_buffer);
	if (cmd_words_len && strstart(cmd_buffer + cmd_words_b[0], "passwd ")) {
		parse_passwd();
	}
}
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
#define RECORD "passwd blake3 1 0078797a 30313233343536373839616263646566"
run("passwd old\n  " RECORD);
assert(passwd_login("pwd=test", sizeof("pwd=test") - 1) == PASSWD_LOGIN_PENDING);
passwd_hash_step();
assert(!passwd_hash_busy && passwd_hash_finish() == PASSWD_LOGIN_ACCEPTED);
run("");
assert(passwd_login("pwd=1234", sizeof("pwd=1234") - 1) == PASSWD_LOGIN_ACCEPTED);
run(RECORD "\npasswd blake3 old");
assert(passwd_login("pwd=blake3+old", sizeof("pwd=blake3+old") - 1) == PASSWD_LOGIN_ACCEPTED);
puts("startup CR/LF/CRLF, comments, NUL, overlong and burst-boundary cases passed");return 0;
}
"""
if sys.argv[1:] == ['--emit']:
    print(harness + tokenize + passwd_handler + body + tests)
else:
    subprocess.run(['make', '-C', str(root / 'test'), 'build/test_startup_crlf'], check=True)
    subprocess.run([root / 'test/build/test_startup_crlf'], check=True)

#!/usr/bin/env python3
"""Real tokenizer/dispatch fragment, Flash startup replay and SNMP engine.
Only Flash and hardware edges mocked. Both compilers use ASAN/UBSAN/-Werror.
"""
from pathlib import Path
import subprocess, os, shlex
r=Path(__file__).resolve().parent.parent;src=(r/'cmd_parser.c').read_text()
def function(signature):
 a=src.index(signature);b=src.index('{',a);depth=1;i=b+1
 while depth:
  if src[i]=='{':depth+=1
  elif src[i]=='}':depth-=1
  i+=1
 return src[a:i]+'\n'
branch=src.split('} else if (cmd_compare(0, "snmp")) {',1)[1].split('#endif',1)[0]
cli=(r/'test/test_snmp_cli.c').read_text().replace('int main(void)','int cli_original_main(void)')
code=cli+'''
#define N_WORDS 15
#define FLASH_READ_BURST_SIZE 256
#define PASSWORD "test-default"
uint8_t replay_image[CONFIG_LEN];extern char passwd[21];
void cmd_error(const char *s){(void)s;err_status=ERR_SYNTAX;}
'''.replace('ERR_SYNTAX','3')+function('uint8_t cmd_compare(')+function('void cmd_tokenize(')
code+='void cmd_parser(void){if(cmd_compare(0,"snmp")){'+branch+'}}\n'+function('void execute_config(')
code+='''
static void boot(const char *s){uip_init();snmp_init();memset(replay_image,0,sizeof replay_image);memcpy(replay_image,s,strlen(s));execute_config();assert(save_cmd==1);}
int main(void){
assert(cli_original_main()==0);
uip_init();snmp_init();assert(test_snmp_configure(comm,sizeof(comm)-1,network,acl));
boot("snmp manager 192.0.3.9/32\\r\\nsnmp community timed-community\\r\\nsnmp changed 1735689600\\r\\nsnmp off\\r\\n");
assert(!snmp_agent_enabled);
assert(!memcmp(snmp_community,"timed-community",15)&&snmp_network[2]==3&&snmp_network[3]==9);
boot("snmp manager 192.0.3.9/32\\nsnmp community timed-community\\nsnmp changed unknown\\nsnmp on\\n");assert(snmp_agent_enabled);
/* Cross a Flash burst boundary and exercise CRLF/no final newline. */
char cross[600];memset(cross,'\\n',255);strcpy(cross+255,"snmp manager 192.0.2.250/32\\nsnmp community restored-value\\nsnmp changed 1735689600\\nsnmp off");
boot(cross);assert(!snmp_agent_enabled&&snmp_network[2]==2&&snmp_network[3]==250);
assert(!memcmp(snmp_community,"restored-value",14));
boot("hostname legacy\\n");assert(!snmp_agent_enabled&&!snmp_community_length&&!snmp_listener);
boot("snmp community restored-value\\nsnmp on\\n");assert(!snmp_agent_enabled); /* Missing ACL fails closed. */
strcpy((char*)cmd_buffer,"snmp on extra");cmd_tokenize();cmd_parser();assert(err_status);
puts("PASS real Flash replay: live engine, CRLF, burst boundary, legacy defaults, legacy metadata ignored, configuration restored");return 0;}
'''
out=r/'test/build/snmp-control-replay';out.mkdir(parents=True,exist_ok=True);c=out/'control.c';c.write_text(code)
for cc in shlex.split(os.environ.get('REPLAY_CC', os.environ.get('CC', 'cc'))):
 exe=out/('test-'+Path(cc).name)
 args=[cc,'-std=gnu11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unknown-pragmas','-Wno-pointer-sign','-DSNMP','-DSNMP_REAL_REPLAY','-DRTLP_HOST_TEST','-include','test/sdcc_shim.h','-Itest/stub','-Itest','-I.','-Ihttpd','-Iuip','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',str(c),'snmp.c','snmp_control.c','uip/uip.c','httpd/httpd.c','-o',str(exe)]
 if 'clang' not in subprocess.check_output([cc,'--version'],text=True):args+=['-Wno-builtin-declaration-mismatch']
 subprocess.run(args,cwd=r,check=True);subprocess.run([str(exe)],check=True)

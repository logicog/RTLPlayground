const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('html/app.js','utf8');
const secret='private-example-value';
function fixture(configured=false,failAt=-1){
 const nodes={},posted=[],toasts=[];let dirty=false,release;
 const $=id=>nodes[id]??={style:{},textContent:'',value:'',checked:false,disabled:false,events:{},addEventListener(type,fn){this.events[type]=fn}};
 const S={info:{snmp:configured?[1,1,192,0,2,250,255,255,255,255]:[0,0,0,0,0,0,0,0,0,0]}};
 const server=Array.from(S.info.snmp);
 const ctx={$,S,toast(message){toasts.push(message)},t(key,args={}){return key+' '+Object.values(args).join(' ')},setDirty(v){dirty=v},
  api(path,opts){assert.equal(path,'/cmd');posted.push(opts.body);const index=posted.length-1;
   const reply=()=>{if(index===failAt)return {ok:false,body:'Rejected: '+opts.body};
    if(opts.body==='snmp on')server[0]=1;
    if(opts.body==='snmp off')server[0]=0;
    if(opts.body.startsWith('snmp community '))server[1]=1;
    if(opts.body.startsWith('snmp manager ')){const [ip,prefix]=opts.body.slice(13).split('/');server.splice(2,4,...ip.split('.').map(Number));let bits=+prefix;for(let i=0;i<4;i++){const n=Math.min(bits,8);server[6+i]=n?256-(1<<(8-n)):0;bits-=n;}}
    return {ok:true,body:'OK'};};
   if(failAt==='network'&&index===2)return Promise.reject(new Error('Network failure: '+opts.body));
   return failAt==='hold'&&index===0?new Promise(resolve=>{release=()=>resolve(reply())}):Promise.resolve(reply());},
  pollInfo(){S.info.snmp=Array.from(server);ctx.renderSnmp(S.info.snmp);return Promise.resolve()}};
 vm.createContext(ctx);
 vm.runInContext(src.slice(src.indexOf('var CONF_CMDS='),src.indexOf('function setDirty(')),ctx);
 vm.runInContext(src.slice(src.indexOf('function postCmd('),src.indexOf('function toast(')),ctx);
 vm.runInContext(src.slice(src.indexOf('function redactSnmp('),src.indexOf('$("sy-cmd").addEventListener("input"')),ctx);
 vm.runInContext(src.slice(src.indexOf('var IPRE='),src.indexOf('function sysLoad(')),ctx);
 ctx.renderSnmp(S.info.snmp);
 function edit(manager,community,enabled){$('sy-snmp-manager').value=manager;$('sy-snmp-secret').value=community;$('sy-snmp-toggle').checked=enabled;$('sy-snmp-manager').events.input();}
 return {ctx,$,posted,toasts,server,edit,dirty:()=>dirty,release:()=>release()};
}
(async()=>{
 let f=fixture();
 assert.equal(f.$('sy-snmp-toggle').disabled,false,'an unconfigured agent can be set up in the form');
 f.edit('192.0.2.250/24',secret,true);await f.ctx.snmpApply();
 assert.deepEqual(f.posted,['snmp off','snmp manager 192.0.2.0/24','snmp community '+secret,'snmp on']);
 assert(f.dirty(),'accepted changes require Save to flash');
 assert.equal(f.$('sy-snmp-manager').value,'192.0.2.0/24');
 assert.equal(f.$('sy-snmp-secret').value,'','accepted community is cleared, never loaded back');
 assert(!f.toasts.join('\n').includes(secret),'successful setup never displays the community');
 f=fixture(true);f.edit('192.0.2.251/32','',true);await f.ctx.snmpApply();
 assert.deepEqual(f.posted,['snmp off','snmp manager 192.0.2.251/32','snmp on'],'blank community retains existing credential');
 f=fixture(true);f.edit('192.0.2.250/32','',false);
 assert.equal(f.posted.length,0,'editing the toggle does not apply before Apply');
 await f.ctx.snmpApply();assert.deepEqual(f.posted,['snmp off']);
 for(const manager of ['0.0.0.0/0','127.0.0.1/32','224.0.0.1/32','1.1.1.1/1','999.1.1.1/24','192.0.2.1/33','192.0.2.1/32\nsnmp on']){
  f=fixture();f.edit(manager,secret,true);await f.ctx.snmpApply();assert.deepEqual(f.posted,[],`invalid ACL blocked before any change: ${manager}`);assert(f.toasts.length);
 }
 for(const community of ['a b','a\nb','a\tb','a\0b','é','a'.repeat(33)]){
  f=fixture();f.edit('192.0.2.250/32',community,true);await f.ctx.snmpApply();assert.deepEqual(f.posted,[],'invalid community blocked before any change');assert(f.toasts.length);
 }
 f=fixture();f.edit('192.0.2.250/32','',true);await f.ctx.snmpApply();assert.deepEqual(f.posted,[],'initial enable requires a community');
 f=fixture(true);f.edit('198.51.100.0/24',secret,false);f.ctx.renderSnmp(f.ctx.S.info.snmp);
 assert.equal(f.$('sy-snmp-manager').value,'198.51.100.0/24','status refresh preserves unsaved manager');
 assert.equal(f.$('sy-snmp-secret').value,secret,'status refresh preserves masked draft');
 assert.equal(f.$('sy-snmp-toggle').checked,false,'status refresh preserves unsaved state');
 f=fixture(false,'hold');f.edit('192.0.2.250/32',secret,true);const pending=f.ctx.snmpApply();await new Promise(setImmediate);
 for(const id of ['sy-snmp-toggle','sy-snmp-manager','sy-snmp-secret','sy-snmp-apply','sy-snmp-refresh'])assert.equal(f.$(id).disabled,true,'pending Apply locks form');
 assert.equal(f.$('saveBtn').disabled,true,'Save cannot snapshot a partially applied configuration');
 await f.ctx.snmpApply();assert.equal(f.posted.length,1,'duplicate Apply cannot start a second update');f.release();await pending;
 assert.equal(f.$('sy-snmp-apply').disabled,false);
 f=fixture(true,2);f.edit('198.51.100.250/32',secret,true);await f.ctx.snmpApply();
 assert.deepEqual(f.posted,['snmp off','snmp manager 198.51.100.250/32','snmp community '+secret],'failed credential update never reenables agent');
 assert.equal(f.server[0],0);assert.equal(f.$('sy-snmp-enabled').textContent,'Disabled');
 assert.equal(f.$('sy-snmp-toggle').checked,false,'partial failure shows actual disabled state, not the unapplied enable request');
 assert.equal(f.$('sy-snmp-apply').disabled,false,'failed update unlocks form for correction');
 assert(!f.toasts.join('\n').includes(secret),'server error echoes never expose community');
 f=fixture(true,'network');f.edit('198.51.100.250/32',secret,true);await f.ctx.snmpApply();
 assert(!f.toasts.join('\n').includes(secret),'network error details never expose community');
 assert.equal(f.server[0],0);assert(!f.posted.includes('snmp on'));
 f=fixture();f.edit('192.168.2.250/9',secret,true);await f.ctx.snmpApply();
 assert(f.posted.includes('snmp manager 192.128.0.0/9'),'non-octet CIDR prefixes normalize host bits');
 f=fixture(true);f.edit('198.51.100.250/32',secret,false);await f.$('sy-snmp-refresh').events.click();
 assert.equal(f.$('sy-snmp-manager').value,'192.0.2.250/32');assert.equal(f.$('sy-snmp-secret').value,'');
 assert.equal(f.$('sy-snmp-toggle').checked,true);assert.deepEqual(f.posted,[],'Refresh discards drafts without changing runtime');
 f=fixture();f.edit('192.0.2.250/32',secret,true);f.ctx.pollInfo=()=>Promise.reject(new Error('unavailable'));await f.ctx.snmpApply();
 assert.equal(f.$('sy-snmp-apply').disabled,false,'failed status refresh cannot lock the form');
 assert.equal(f.$('saveBtn').disabled,false);assert(!f.toasts.join('\n').includes(secret));
 f.ctx.renderSnmp(undefined);assert.equal(f.$('sy-snmp').style.display,'none');
 console.log('PASS SNMP form: setup, preserved community, validation, drafts, pending requests, fail-closed errors and credential privacy');
})().catch(e=>{console.error(e);process.exitCode=1});

const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('html/app.js','utf8'),nodes={};
const $=id=>nodes[id]??={value:'',textContent:'',addEventListener(e,f){this[e]=f}};
let modalNodes,posted,body='',denied=false;
const ctx={$,Blob,t:(x,o)=>x+(o?JSON.stringify(o):''),toast(){},cfgBytes(){},cfgParseKnown(){},closeModal(){},setDirty(){},
 h(tag,attrs,children){return {tag,...attrs,value:'',style:{},children,addEventListener(e,f){this[e]=f}}},
 modal(t,content,buttons){modalNodes={content,buttons}},api(){return denied?Promise.reject(Error('denied')):Promise.resolve({ok:true,body})},
 doWriteConfig(raw){posted=raw}};vm.createContext(ctx);
vm.runInContext(src.slice(src.indexOf('var CONF_CMDS='),src.indexOf('function setDirty(')),ctx);
vm.runInContext(src.slice(src.indexOf('var cfgHidden='),src.indexOf('function cfgReload(')),ctx);
vm.runInContext(src.slice(src.indexOf('function cfgLongLine('),src.indexOf('function cfgBytes(')),ctx);
vm.runInContext(src.slice(src.indexOf('function writeConfig('),src.indexOf('function doWriteConfig(')),ctx);
vm.runInContext(src.slice(src.indexOf('function sysConsole('),src.indexOf('$("sy-reboot")')),ctx);
const secret='private-example-value',raw='hostname lab\r\nsnmp community '+secret+'\r\nsnmp changed unknown\r\nsnmp on\r\n';
ctx.setConfigEditor(raw);assert(!$('cfgedit').value.includes(secret));assert($('cfgedit').value.includes('[hidden:1]'));
assert.equal(ctx.unmaskConfig($('cfgedit').value,ctx.cfgHidden),raw.replace(/\r\n/g,'\n'));
assert.throws(()=>ctx.unmaskConfig('snmp community new-value',ctx.cfgHidden),/CLI/);
for(const fromLog of [false,true]){ctx.writeConfig(fromLog?raw:$('cfgedit').value,'save',fromLog);
 const ed=modalNodes.content.children[2];assert(!ed.value.includes(secret));assert(!JSON.stringify(modalNodes).includes(secret));
 modalNodes.buttons[1].onclick();assert.equal(posted,raw.replace(/\r\n/g,'\n'));
}
ctx.writeConfig('snmp community bad\tvalue\n','save',true);assert(!modalNodes.content.children[1].textContent.includes('bad\tvalue'));
const r=ctx.redactSnmp('> snmp community '+secret+'\r\nsnmp community '+secret+'\r\nOK');assert(!r.includes(secret));
$('sy-cmd').value='snmp community '+secret;$('sy-cmd').input.call($('sy-cmd'));assert.equal($('sy-cmd').type,'password');
(async()=>{body='snmp community '+secret+'\r\nOK';ctx.sysConsole();await new Promise(setImmediate);assert(!$('sy-cout').textContent.includes(secret));assert.equal($('sy-cmd').value,'');assert.equal($('sy-cmd').type,'text');
 denied=true;$('sy-cmd').value='snmp community '+secret;ctx.sysConsole();await new Promise(setImmediate);assert(!$('sy-cout').textContent.includes(secret));
 console.log('PASS credential masking: CRLF editor, manual/Save dialogs, round-trip preservation, invalid editing, console success/history/error and password input');
})().catch(e=>{console.error(e);process.exitCode=1});

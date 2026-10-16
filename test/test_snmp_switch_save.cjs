/* Actual Save callback must use the live configuration snapshot. */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('html/app.js','utf8');let handler,saved=[],state=0,fail=false;
const context={S:{info:{snmp:[1,1,192,0,2,250,255,255,255,255]}},
 $(){return {addEventListener(e,fn){handler=fn}}},t:x=>x,toast(){},
 getText(path){if(path==='/config')return Promise.resolve('hostname lab\nsnmp on\nsnmp community stale-value\nsnmp changed 1735689500\nsnmp future retained\n');
 if(path==='/cmd_log')return Promise.resolve('snmp community history-stale\nsnmp off\n');
 if(path==='/snmp_config')return fail?Promise.reject(new Error('unavailable')):Promise.resolve('snmp manager 192.0.2.250/32\nsnmp community current-value\nsnmp '+(state?'on':'off')+'\n');throw Error(path)},
 writeConfig(text,title,fromLog){saved.push({text,title,fromLog})}};
vm.createContext(context);
vm.runInContext(src.slice(src.indexOf('var CONF_CMDS='),src.indexOf('function setDirty(')),context);
vm.runInContext(src.slice(src.indexOf('var CONF_OVERWRITE='),src.indexOf('function writeConfig(')),context);
vm.runInContext(src.slice(src.indexOf('$("saveBtn").addEventListener('),src.indexOf('var fwBuf=')),context);
(async()=>{handler();await new Promise(setImmediate);
 const prefix='hostname lab\nsnmp future retained\nsnmp manager 192.0.2.250/32\nsnmp community current-value\nsnmp ';
 assert.equal(saved[0].text,prefix+'off');assert(!saved[0].text.includes('snmp changed'));assert.equal(saved[0].fromLog,true);
 state=1;handler();await new Promise(setImmediate);assert.equal(saved[1].text,prefix+'on');
 fail=true;handler();await new Promise(setImmediate);assert.equal(saved.length,2);
 context.S.info={};handler();await new Promise(setImmediate);assert(saved[2].text.includes('history-stale'),'disabled build preserves legacy merging');
 console.log('PASS Save: atomic live snapshot overrides stale history/config, future entries retained, failures abort, disabled build preserved');
})().catch(e=>{console.error(e);process.exitCode=1});

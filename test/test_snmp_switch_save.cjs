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
 // Exercise the real Save callback with both independent toggle families.
 fail=false;context.S.info={snmp:[1,1,192,0,2,250,255,255,255,255]};
 let startup='hostname lab\nlldp off\nsnmp off\nport 4 name desk\nport 4 lldp block\n';
 let commands='lldp on\nport 4 lldp permit\nport 4 auto\nsnmp off\n';
 const liveGet=context.getText;
 context.getText=function(path){
  if(path==='/config')return Promise.resolve(startup);
  if(path==='/cmd_log')return Promise.resolve(commands);
  return liveGet(path);
 };
 for(const [enabled,lldpEnabled] of [[true,false],[false,true],[true,true],[false,false]]){
  state=+enabled;
  commands='lldp '+(lldpEnabled?'on':'off')+'\nport 4 lldp '+(lldpEnabled?'permit':'block')+'\nport 4 auto\nsnmp '+(enabled?'off':'on')+'\n';
  handler();await new Promise(setImmediate);
  const result=saved.at(-1),lines=result.text.split('\n');
  assert.deepEqual(lines.filter(x=>/^lldp /.test(x)),['lldp '+(lldpEnabled?'on':'off')]);
  assert.deepEqual(lines.filter(x=>/^snmp (on|off)$/.test(x)),['snmp '+(enabled?'on':'off')]);
  assert.deepEqual(lines.filter(x=>/^port 4 lldp /.test(x)),['port 4 lldp '+(lldpEnabled?'permit':'block')]);
  assert(lines.includes('port 4 name desk'));assert(lines.includes('port 4 auto'));
  assert(lines.includes('snmp community current-value'));assert.equal(result.fromLog,true);
  // Reload persisted text, then save it again: no duplicate or lost settings.
  startup=result.text;commands='';handler();await new Promise(setImmediate);
  assert.deepEqual(saved.at(-1),result);
 }
 console.log('PASS combined LLDP/SNMP Save: both toggle directions, per-port LLDP/name/speed preservation, live SNMP snapshot, persisted reload and idempotence');
 console.log('PASS Save: atomic live snapshot overrides stale history/config, future entries retained, failures abort, disabled build preserved');
})().catch(e=>{console.error(e);process.exitCode=1});

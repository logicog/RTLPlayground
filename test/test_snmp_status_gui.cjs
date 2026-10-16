/* Isolated renderer test; no browser, network, or external libraries. */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('html/app.js','utf8');
const render=src.slice(src.indexOf('var snmpPending='),src.indexOf('function snmpToggle('));
const nodes={};const $=id=>nodes[id]??=( {style:{},textContent:''} );
const context={$};vm.createContext(context);vm.runInContext(render,context);
context.renderSnmp([1,1,192,0,2,250,255,255,255,255]);
assert.equal($('sy-snmp-enabled').textContent,'Enabled');
assert.equal($('sy-snmp-network').textContent,'192.0.2.250');
assert.equal($('sy-snmp-mask').textContent,'255.255.255.255');
assert.equal($('sy-snmp-community').textContent,'Configured');
assert.equal($('sy-snmp-toggle').checked,true);assert.equal($('sy-snmp-toggle').disabled,false);
context.renderSnmp([0,0,0,0,0,0,0,0,0,0]);
assert.equal($('sy-snmp-enabled').textContent,'Disabled');
assert.equal($('sy-snmp-community').textContent,'Not configured');
assert.equal($('sy-snmp-toggle').checked,false);assert.equal($('sy-snmp-toggle').disabled,true);
context.renderSnmp(undefined);assert.equal($('sy-snmp').style.display,'none');
const html=fs.readFileSync('html/index.html','utf8');
const start=html.indexOf('<div class="card" id="sy-snmp"');
let depth=0,end=start;
for(const m of html.slice(start).matchAll(/<\/?div\b[^>]*>/g)){depth+=m[0].startsWith('</')?-1:1;if(!depth){end=start+m.index+m[0].length;break}}
const card=html.slice(start,end);
assert(card.length>0);
assert.equal((card.match(/<input\b/g)||[]).length,1);
assert(card.includes('id="sy-snmp-toggle"'));
assert(!/<(?:button|select|textarea)\b/.test(card));
assert(card.includes('Read-Only')&&card.includes('161')&&card.includes('SNMPv2c'));
console.log('PASS runtime renderer, disabled-build omission and bounded non-secret agent switch');

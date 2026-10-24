/* Isolated renderer test; no browser, network, or external libraries. */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('html/app.js','utf8');
const render=src.slice(src.indexOf('var snmpPending='),src.indexOf('function snmpManagerValue('));
const nodes={};const $=id=>nodes[id]??={style:{},textContent:'',value:''};
const context={$};vm.createContext(context);vm.runInContext(render,context);
context.renderSnmp([1,1,192,0,2,250,255,255,255,255]);
assert.equal($('sy-snmp-manager').value,'192.0.2.250/32');
assert.equal($('sy-snmp-secret').value,'','stored credentials are never filled into the form');
assert.equal($('sy-snmp-toggle').checked,true);assert.equal($('sy-snmp-toggle').disabled,false);
context.renderSnmp([0,0,0,0,0,0,0,0,0,0]);
assert.equal($('sy-snmp-manager').value,'');assert.equal($('sy-snmp-secret').value,'');
assert.equal($('sy-snmp-toggle').checked,false);assert.equal($('sy-snmp-toggle').disabled,false);
context.renderSnmp(undefined);assert.equal($('sy-snmp').style.display,'none');
console.log('PASS runtime CIDR rendering, credential withholding, initial setup and disabled-build omission');

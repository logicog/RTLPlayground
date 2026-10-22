/* The supported SNMP configuration has one entry point in System settings. */
const fs=require('node:fs'),assert=require('node:assert/strict');
const html=fs.readFileSync('html/index.html','utf8');
const js=fs.readFileSync('html/app.js','utf8');

assert.equal((html.match(/id="sy-snmp"/g)||[]).length,1,'one SNMP settings page');
assert.equal((html.match(/id="sy-snmp-(?:toggle|manager|secret|apply|refresh)"/g)||[]).length,5,
  'one set of SNMP configuration controls');
assert(!/id="tab-snmp"|data-tab="snmp"/.test(html),'no legacy SNMP navigation tab');
assert(!/id="snmp-(?:config|settings)"/.test(html),'no duplicate legacy SNMP configuration page');
assert(html.includes('.row>.in{flex:1;min-width:0;width:0}'),'form fields stay within the settings row');
assert(js.includes('var supported=Array.isArray(s)&&s.length===10;')&&
  js.includes('$("sy-snmp").style.display=supported?"":"none";'),
  'SNMP page is hidden when firmware omits SNMP status');
assert.equal((js.match(/\$\("sy-snmp-apply"\)\.addEventListener\("click",snmpApply\)/g)||[]).length,1,
  'one Apply handler');
console.log('PASS unified SNMP settings entry point, no legacy tab, unique controls and disabled-build hiding');

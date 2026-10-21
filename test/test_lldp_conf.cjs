/* Save to flash keeps the LLDP commands: mergeConf() of html/app.js. */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('html/app.js','utf8'),context={};
vm.createContext(context);
vm.runInContext(src.slice(src.indexOf('var CONF_CMDS='),src.indexOf('function setDirty(')),context);
vm.runInContext(src.slice(src.indexOf('var CONF_OVERWRITE='),src.indexOf('function writeConfig(')),context);
const {mergeConf,isConfCmd}=context;

assert(isConfCmd('lldp tlv macphy off')&&isConfCmd('lldp tlv portdesc on')&&isConfCmd('port 3 lldp block'));
assert(!isConfCmd('lldp tlv bogus on')&&!isConfCmd('lldp tlv mfs')&&!isConfCmd('lldp'));
const out=mergeConf(['port 3 1g','port 3 name uplink','lldp on','port 3 lldp block','lldp tlv mfs off','port 4 lldp block'],
  ['port 3 2g5\nport 3 lldp permit\nlldp tlv mfs on\nlldp tlv pvid off\nlldp off\nport 4 off']);
assert.deepEqual(out,['port 3 name uplink','port 4 lldp block','port 3 2g5','port 3 lldp permit',
  'lldp tlv mfs on','lldp tlv pvid off','lldp off','port 4 off']);
console.log('PASS LLDP: tlv and per-port commands kept, latest setting wins, port speed and lldp do not overwrite each other');

/* Save to flash keeps one "lldp tlv" line per TLV: mergeConf() of html/app.js. */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const src=fs.readFileSync('html/app.js','utf8'),context={};
vm.createContext(context);
vm.runInContext(src.slice(src.indexOf('var CONF_CMDS='),src.indexOf('function setDirty(')),context);
vm.runInContext(src.slice(src.indexOf('var CONF_OVERWRITE='),src.indexOf('function writeConfig(')),context);
const {mergeConf,isConfCmd}=context;

assert(isConfCmd('lldp tlv portdesc off')&&isConfCmd('lldp tlv lag on')&&isConfCmd('lldp tlv mfs off'));
assert(!isConfCmd('lldp tlv bogus on')&&!isConfCmd('lldp tlv mfs'));
const first=mergeConf(['lldp on','port 3 name uplink'],['lldp tlv mfs off\nlldp tlv pvid off\nlldp tlv mfs on\nlldp tlv mfs off']);
assert.deepEqual(first,['lldp on','port 3 name uplink','lldp tlv pvid off','lldp tlv mfs off']);
const second=mergeConf(first,['lldp tlv pvid on\nlldp tlv syscap off\nlldp tlv mfs off']);
assert.deepEqual(second,['lldp on','port 3 name uplink','lldp tlv pvid on','lldp tlv syscap off','lldp tlv mfs off']);
console.log('PASS LLDP TLVs: two saves keep one line per TLV, the latest setting wins');

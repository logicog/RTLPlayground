/* Exercise the web app's real configuration merger without a DOM or dependencies. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../html/app.js'), 'utf8');
const context = vm.createContext({});
vm.runInContext(
  source.slice(source.indexOf('var CONF_CMDS='), source.indexOf('function setDirty')) +
  source.slice(source.indexOf('var CONF_OVERWRITE='), source.indexOf('function writeConfig')),
  context
);
function merge(base, log) {
  return Array.from(context.mergeConf(base, [log]));
}
let failed = 0;
const cases = [
  ['a replaced parent still precedes its child',
    ['acl 1 udp permit', 'acl 2 and dport 53', 'acl default drop 1'],
    'acl 1 tcp permit',
    ['acl default drop 1', 'acl 1 tcp permit', 'acl 2 and dport 53']],
  ['turning an earlier independent rule into a child preserves replay order',
    ['acl 2 drop', 'acl 1 udp permit'], 'acl 2 and dport 53',
    ['acl 1 udp permit', 'acl 2 and dport 53']],
  ['off removes a whole chain but keeps the next independent rule',
    ['acl 9 udp permit', 'acl 10 and dport 53', 'acl 11 and sip 192.0.2.1', 'acl 12 tcp drop'],
    'acl 9 off', ['acl 12 tcp drop']],
  ['off followed by a replacement does not resurrect children',
    ['acl 1 udp permit', 'acl 2 and dport 53'], 'acl 1 off\nacl 1 tcp permit',
    ['acl 1 tcp permit']],
  ['removing an interior child retains its parent and removes later children',
    ['acl 1 udp permit', 'acl 2 and dport 53', 'acl 3 and sip 192.0.2.1'],
    'acl 2 off', ['acl 1 udp permit']],
  ['field disable replaces the saved enabled selector',
    ['acl field 3 ipv6 8'], 'acl field 3 off', ['acl field 3 off']],
  ['explicit ACL reservations precede dynamic storm allocation after save',
    ['acl 1 police 63', 'storm 1 bcast 100 pps'], 'acl 1 police 63',
    ['acl 1 police 63', 'storm 1 bcast 100 pps']],
  ['leading-zero rule IDs are removed with their children',
    ['acl 01 udp permit', 'acl 02 and dport 53'], 'acl 01 off', []],
  ['mixed spellings replace the same numeric rule',
    ['acl 01 udp permit', 'acl 02 and dport 53'], 'acl 1 tcp permit',
    ['acl 1 tcp permit', 'acl 2 and dport 53']],
  ['editor-written base commands are reduced before sorting',
    ['vendor setup', 'acl 1 udp permit', 'acl 2 and dport 53', 'acl 1 off', 'acl 1 tcp permit'],
    '', ['vendor setup', 'acl 1 tcp permit']],
];
for (const [name, base, log, want] of cases) {
  try {
    assert.deepEqual(merge(base, log), want);
    console.log('ok: ' + name);
  } catch (error) {
    failed++;
    console.error('FAIL: ' + name + '\n' + error.message);
  }
}
try {
  assert.equal(context.isConfCmd('acl field 3 off'), true);
  assert.equal(context.isConfCmd('acl field 3 off 0'), true);
  assert.equal(context.isConfCmd('acl field 3 ipv6'), false);
  console.log('ok: selector disable marks changes dirty without accepting a missing active offset');
} catch (error) {
  failed++;
  console.error('FAIL: selector command classification\n' + error.message);
}
process.exitCode = failed ? 1 : 0;

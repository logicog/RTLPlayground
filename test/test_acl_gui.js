/* Exercise the production ACL helpers against measured firmware encodings.
 * Run with: node test/test_acl_gui.js
 * No browser, packages or network access are needed. */
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const app = fs.readFileSync(path.join(__dirname, "../html/app.js"), "utf8");
function block(start, end) {
  const a = app.indexOf(start), b = app.indexOf(end, a + start.length);
  assert(a >= 0 && b > a, "Production helper boundaries exist: " + start);
  return app.slice(a, b);
}
const context = vm.createContext({
  S: {
    n: 9, ports: Array.from({length: 9}, (_, i) => ({portNum: i + 1, logPort: i})),
    physToLog: [0, 1, 2, 3, 4, 5, 6, 7, 8], logToPhys: [1, 2, 3, 4, 5, 6, 7, 8, 9]
  },
  portNum: p => p.portNum,
  t: key => key,
  toast: () => {},
  setDirty: dirty => { context.S.dirty = dirty; },
  console
});
vm.runInContext(block("/* ACL helpers */", "/* ACL UI */"), context);
vm.runInContext(block("var CONF_CMDS", "function setDirty("), context);
vm.runInContext(block("var CONF_OVERWRITE", "function writeConfig("), context);
vm.runInContext(block("function postCmd(", "function toast("), context);
const plain = obj => JSON.parse(JSON.stringify(obj));

const matchFlags = new Set([
  "tagged", "untagged", "stagged", "pppoe", "nonip", "arp", "ipv4", "ipv6",
  "tcp", "udp", "icmp", "igmp", "l4other"
]);
const matchValues = new Set([
  "dmac", "smac", "ethertype", "vlan", "pri", "cfi", "svlan", "spri", "sdei",
  "sip", "dip", "sip6", "dip6", "proto", "tos", "sport", "dport", "valid"
]);
const actionValues = new Set([
  "redirect", "copy", "mirror", "isolate", "setvlan", "outvlan", "setsvlan", "outsvlan",
  "priority", "pcp", "dscp", "police", "count", "gpio", "bypass"
]);
const portActions = new Set(["redirect", "copy", "mirror", "isolate"]);

/* Independent parser for the deliberately limited, valid measured fixture CLI.
 * It supplies expected semantics rather than duplicating the hardware decoder. */
function parseFixtureCommand(command) {
  const words = command.split(" ");
  assert.equal(words.shift(), "acl");
  const model = {n: +words.shift(), mode: "normal", matches: [], actions: [], ports: []};
  if (words[0] === "and" || words[0] === "not") model.mode = words.shift();
  while (words.length) {
    const key = words[0];
    if (matchFlags.has(key)) {
      words.shift(); model.matches.push({key, value: ""});
    } else if (matchValues.has(key)) {
      words.shift(); model.matches.push({key, value: words.shift()});
    } else if (key === "field") {
      words.shift(); model.matches.push({key, index: +words.shift(), value: words.shift()});
    } else break;
  }
  while (words.length && !/^\d+$/.test(words[0])) {
    const key = words.shift();
    let value = "";
    if (actionValues.has(key)) value = words.shift();
    if (key === "trap") value = /^(int|ext|both)$/.test(words[0]) ? words.shift() : "int";
    if (portActions.has(key)) value = value.split(",").map(Number);
    model.actions.push({key, value});
  }
  model.ports = words.map(Number);
  return model;
}

function ipv4(s) { return s.split(".").reduce((v, octet) => v * 256 + +octet, 0); }
function signature(term) {
  const key = term.key;
  let value = term.value == null ? "" : String(term.value).toLowerCase();
  if (matchFlags.has(key)) value = "";
  if (key === "trap") value = value || "int";
  if (key === "dmac" || key === "smac") {
    const parts = value.split("/");
    value = parts[0].replace(/-/g, ":") + "/" + (parts[1] || "ff:ff:ff:ff:ff:ff").replace(/-/g, ":");
  } else if (key === "ethertype" || key === "tos" || key === "field") {
    const parts = value.split("/");
    const mask = parts[1] === undefined ? (key === "tos" ? 255 : 65535) : parseInt(parts[1], 16);
    value = (parseInt(parts[0], 16) & mask) + "/" + mask;
  } else if (key === "sip" || key === "dip") {
    if (value.includes("/")) {
      const parts = value.split("/"), size = 2 ** (32 - +parts[1]);
      const low = Math.floor(ipv4(parts[0]) / size) * size;
      value = low + "-" + (low + size - 1);
    } else {
      const parts = value.split("-");
      value = ipv4(parts[0]) + "-" + ipv4(parts[1] || parts[0]);
    }
  } else if (["sip6", "dip6", "vlan", "svlan", "sport", "dport"].includes(key)) {
    const parts = value.split("-"), base = key.endsWith("ip6") ? 16 : 10;
    value = parseInt(parts[0], base) + "-" + parseInt(parts[1] || parts[0], base);
  } else if (portActions.has(key) || key === "police") {
    value = (Array.isArray(term.value) ? term.value : value.split(",")).map(Number).sort((a, b) => a - b).join(",");
  } else if (/^\d+$/.test(value)) value = String(+value);
  return key + (key === "field" ? ":" + +term.index : "") + "=" + value;
}
function semantic(model) {
  let ports = (model.ports || []).map(Number).sort((a, b) => a - b);
  if (ports.length === context.S.ports.length && context.S.ports.every(p => ports.includes(p.portNum))) ports = [];
  return {
    n: +model.n, mode: model.mode,
    matches: model.matches.map(signature).sort(), actions: model.actions.map(signature).sort(), ports
  };
}

const templates = [
  [0, 1, 2, 3, 4, 5, 6, 8], [16, 17, 18, 19, 52, 53, 54, 8],
  [48, 49, 50, 51, 64, 65, 66, 67], [68, 69, 70, 71, 72, 73, 74, 75],
  [76, 77, 78, 79, 7, 8, 6, 52]
];
assert.deepEqual(plain(context.ACL_TEMPLATES), templates, "Browser templates match independently measured hardware layouts");
const hex = n => (n >>> 0).toString(16).padStart(8, "0");
const ranges = Array.from({length: 3}, () => Array.from({length: 16}, () => ({type: 0, lo: "00000000", hi: "00000000"})));
ranges[1][0] = {type: 1, lo: "0a090000", hi: "0a0900ff"};
ranges[1][1] = {type: 3, lo: "00000001", hi: "00000001"};
ranges[2][0] = ranges[2][1] = {type: 2, lo: hex(1000), hi: hex(2000)};

/* Keep a single source of independently measured words: the existing C tests. */
const cFixtures = fs.readFileSync(path.join(__dirname, "test_acl.c"), "utf8");
const fixturePattern = /\{"(acl [^"]+)", (\d+), \{([^}]+)\}, \{([^}]+)\}, \{([^}]+)\}, (0x[\da-f]+)\}/gi;
let fixtureCount = 0;
for (const fixture of cFixtures.matchAll(fixturePattern)) {
  const [, command, idx, xText, yText, actText, ctrl] = fixture;
  const nums = s => s.split(",").map(n => Number(n.trim()));
  const x = nums(xText), y = nums(yText), act = nums(actText);
  const care = x.map((word, i) => (~(word & y[i])) >>> 0);
  const template = y[4] & 7;
  const fields = templates[template].map((ft, i) => {
    const shift = (i & 1) * 16, word = i >> 1;
    const mask = (care[word] >>> shift) & 65535;
    return [(y[word] >>> shift) & mask, mask];
  });
  const rule = {
    n: +idx + 1, t: template, ctrl: Number(ctrl),
    info: [y[4] & 65535, y[4] >>> 16, care[4] & 65535, care[4] >>> 16],
    fields, act: act.map(hex)
  };
  const expected = parseFixtureCommand(command);
  const decoded = plain(context.aclDecodeRule(rule, ranges));
  assert.deepEqual(semantic(decoded), semantic(expected), "Decode measured rule: " + command);
  assert.deepEqual(semantic(parseFixtureCommand(context.aclRuleCommand(expected))), semantic(expected), "Serialize measured rule: " + command);
  assert.deepEqual(semantic(parseFixtureCommand(context.aclRuleCommand(decoded))), semantic(expected), "Roundtrip measured rule: " + command);
  fixtureCount++;
}
assert(fixtureCount >= 20, "All measured C fixtures were found");

const validSelectors = {
  n: 1, mode: "normal", matches: [{key: "valid", value: "3"}, {key: "valid", value: "15"}],
  actions: [{key: "permit", value: ""}], ports: []
};
assert.deepEqual(semantic(parseFixtureCommand(context.aclRuleCommand(validSelectors))), semantic(validSelectors), "A rule can test several selector valid bits");
assert.throws(() => context.aclRuleCommand({...validSelectors, matches: [{key: "valid", value: "3"}, {key: "valid", value: "3"}]}), "The same selector valid bit cannot be repeated");
const validRule = {
  n: 1, t: 2, ctrl: 32, info: [0xfffa, 0xffef, 7, 0x10],
  fields: templates[2].map(ft => [ft === 51 ? 0x8008 : 0, ft === 51 ? 0x8008 : 0]),
  act: ["00000000", "00000000", "00000000"]
};
assert.deepEqual(semantic(plain(context.aclDecodeRule(validRule, ranges))), semantic(validSelectors), "Decode each active selector-valid bit");

const originalPorts = plain(context.S);
context.S.n = 2;
context.S.ports = [{portNum: 1, logPort: 3}, {portNum: 2, logPort: 6}];
context.S.physToLog = [3, 6];
context.S.logToPhys = [0, 0, 0, 1, 0, 0, 2];
const mappedRule = {
  n: 1, t: 0, ctrl: 32, info: [0, 2, 7, 0x10],
  fields: templates[0].map(() => [0, 0]), act: ["00000000", "01020000", "00000000"]
};
const mappedModel = plain(context.aclDecodeRule(mappedRule, ranges));
assert.deepEqual(mappedModel.ports, [2], "Decode ingress using machine physical/logical mapping");
assert.deepEqual(mappedModel.actions, [{key: "redirect", value: [1]}], "Decode egress using machine physical/logical mapping");
assert.equal(context.aclRuleCommand(mappedModel), "acl 1 redirect 1 2", "CLI uses physical ports on machines with nonidentity mapping");
Object.assign(context.S, originalPorts);

const compactRanges = plain(ranges);
compactRanges[1][0] = {type: 1, lo: "ffffff80", hi: "ffffffff"};
compactRanges[1][1] = {type: 2, lo: "ffffff80", hi: "ffffffff"};
compactRanges[2][0] = {type: 2, lo: "0000fffe", hi: "0000ffff"};
const compactFields = {49: [3, 3], 50: [1, 1], 51: [0x8000, 0x8000], 64: [65535, 65535]};
const compactRule = {
  n: 64, t: 2, ctrl: 64, info: [0xfffa, 0xffef, 7, 0x10],
  fields: templates[2].map(ft => compactFields[ft] || [0, 0]),
  act: ["00000000", "80000000", "00000000"]
};
const compactModel = parseFixtureCommand("acl 64 sip 255.255.255.128/25 dip 255.255.255.128/25 dport 65534-65535 field 0 ffff valid 15 interrupt");
const compactDecoded = plain(context.aclDecodeRule(compactRule, compactRanges));
assert.deepEqual(semantic(compactDecoded), semantic(compactModel), "Decode prefix matches represented by hardware ranges");
assert.deepEqual(semantic(parseFixtureCommand(context.aclRuleCommand(compactDecoded))), semantic(compactModel), "Editing a valid long rule preserves compact prefix representation");

function rule(matches = [], actions = [{key: "permit", value: ""}], ports = []) {
  return {n: 1, mode: "normal", matches, actions, ports};
}
const validMatches = [
  ...Array.from(matchFlags, key => ({key, value: ""})),
  {key: "dmac", value: "00:11:22:33:44:55"}, {key: "smac", value: "00:11:22:33:44:55/ff:ff:ff:00:00:00"},
  {key: "ethertype", value: "0800/ffff"}, {key: "vlan", value: "0-4095"}, {key: "svlan", value: "4095"},
  {key: "pri", value: "7"}, {key: "cfi", value: "1"}, {key: "spri", value: "7"}, {key: "sdei", value: "0"},
  {key: "sip", value: "0.0.0.0/0"}, {key: "dip", value: "192.0.2.0-192.0.2.255"},
  {key: "sip6", value: "0-ffffffff"}, {key: "dip6", value: "ffffffff"}, {key: "proto", value: "255"},
  {key: "tos", value: "ff/fc"}, {key: "sport", value: "0-65535"}, {key: "dport", value: "65535"},
  {key: "valid", value: "15"}, {key: "field", index: 15, value: "0000/ffff"}
];
for (const match of validMatches) {
  const model = rule([match]);
  assert.deepEqual(semantic(parseFixtureCommand(context.aclRuleCommand(model))), semantic(model), "Serialize match " + match.key);
}
const validActions = [
  ...["drop", "permit", "cpu", "cvidfromsvid", "svidfromcvid", "tag", "untag", "keeptag", "keepremark", "interrupt"].map(key => ({key, value: ""})),
  ...["redirect", "copy", "mirror", "isolate"].map(key => ({key, value: [1, 9]})),
  ...["int", "ext", "both"].map(value => ({key: "trap", value})),
  ...["setvlan", "outvlan", "setsvlan", "outsvlan"].map(key => ({key, value: "4095"})),
  {key: "priority", value: "7"}, {key: "pcp", value: "7"}, {key: "dscp", value: "63"},
  {key: "police", value: "0,31,63"}, {key: "count", value: "31"}, {key: "gpio", value: "3"},
  ...["storm", "stp", "vlan"].map(value => ({key: "bypass", value}))
];
for (const action of validActions) {
  const model = rule([], [action]);
  assert.deepEqual(semantic(parseFixtureCommand(context.aclRuleCommand(model))), semantic(model), "Serialize action " + action.key + " " + action.value);
}

const invalidMatches = [
  {key: "dmac", value: "00:11:22:33:44"}, {key: "ethertype", value: "10000"},
  {key: "vlan", value: "4096"}, {key: "svlan", value: "20-10"}, {key: "pri", value: "8"},
  {key: "cfi", value: "2"}, {key: "spri", value: "-1"}, {key: "sdei", value: "2"},
  {key: "sip", value: "256.0.0.1"}, {key: "dip", value: "192.0.2.0/33"},
  {key: "sip", value: "192.0.2.2-192.0.2.1"}, {key: "sip6", value: "100000000"},
  {key: "dip6", value: "ff-00"}, {key: "proto", value: "256"}, {key: "tos", value: "100"},
  {key: "sport", value: "65536"}, {key: "dport", value: "2-1"}, {key: "valid", value: "16"},
  {key: "field", index: 16, value: "ffff"}, {key: "field", index: 0, value: "ffff/10000"},
  {key: "unknown", value: "0"}
];
for (const match of invalidMatches) assert.throws(() => context.aclRuleCommand(rule([match])), "Reject match " + JSON.stringify(match));
const invalidActions = [
  {key: "redirect", value: []}, {key: "copy", value: [10]}, {key: "trap", value: "bogus"},
  {key: "setvlan", value: "4096"}, {key: "priority", value: "8"}, {key: "pcp", value: "8"},
  {key: "dscp", value: "64"}, {key: "police", value: "1,2,3,4"}, {key: "police", value: "64"},
  {key: "count", value: "32"}, {key: "gpio", value: "4"}, {key: "bypass", value: "unknown"}
];
for (const action of invalidActions) assert.throws(() => context.aclRuleCommand(rule([], [action])), "Reject action " + JSON.stringify(action));
for (const n of [0, 65, 1.5]) assert.throws(() => context.aclRuleCommand({...rule(), n}), "Reject rule number " + n);
assert.throws(() => context.aclRuleCommand(rule([], [])), "Normal rules need an action");
assert.throws(() => context.aclRuleCommand(rule([], [{key: "drop"}, {key: "permit"}])), "Two forwarding actions conflict");
assert.throws(() => context.aclRuleCommand(rule([], [{key: "pcp", value: "1"}, {key: "dscp", value: "2"}])), "Two remarks conflict");
assert.throws(() => context.aclRuleCommand(rule([], [{key: "police", value: "1"}, {key: "count", value: "2"}])), "Police and count conflict");
assert.throws(() => context.aclRuleCommand(rule([], [{key: "police", value: "1,2"}, {key: "outvlan", value: "2"}])), "Meter two uses C-VLAN action");
assert.throws(() => context.aclRuleCommand(rule([], [{key: "police", value: "1,2,3"}, {key: "outsvlan", value: "2"}])), "Meter three uses S-VLAN action");
assert.throws(() => context.aclRuleCommand(rule([], [{key: "drop"}], [10])), "Reject absent ingress port");
assert.throws(() => context.aclRuleCommand({...rule(), n: 1, mode: "and", actions: []}), "AND cannot be rule one");
assert.throws(() => context.aclRuleCommand({...rule(), n: 2, mode: "and"}), "AND has no actions");

/* Independent command constraints from the firmware tokenizer and buffer. */
const fourteenWords = rule([{key: "udp"}, {key: "dport", value: "53"}], [{key: "count", value: "0"}], [1, 2, 3, 4, 5, 6, 7]);
assert.equal(context.aclRuleCommand(fourteenWords).split(" ").length, 14);
assert.throws(() => context.aclRuleCommand({...fourteenWords, ports: [1, 2, 3, 4, 5, 6, 7, 8]}), "Reject a fifteenth word");
const longRule = rule([
  {key: "dmac", value: "00:11:22:33:44:55/ff:ff:ff:ff:ff:ff"},
  {key: "smac", value: "00:11:22:33:44:55/ff:ff:ff:ff:ff:ff"},
  {key: "sip", value: "192.168.100.100-192.168.100.200"}
], [{key: "redirect", value: [1, 2, 3, 4, 5, 6, 7, 8, 9]}]);
assert.throws(() => context.aclRuleCommand(longRule), "Reject a command exceeding 126 bytes even below the word limit");

assert.deepEqual(plain(context.mergeConf([
  "acl 1 drop", "acl 2 and udp", "acl 3 and dport 53", "acl 4 permit"
], ["acl 1 off"])), ["acl 4 permit"], "Persistence removes the AND chain");
assert.deepEqual(plain(context.mergeConf([
  "acl 1 drop", "acl 2 and udp", "acl 3 and dport 53", "acl 4 permit"
], ["acl 2 off"])), ["acl 1 drop", "acl 4 permit"], "Deleting an AND entry preserves its predecessor");
assert.deepEqual(plain(context.mergeConf([
  "acl 1 drop", "acl 2 and udp", "acl 3 permit"
], ["acl 1 count 0"])), ["acl 1 count 0", "acl 2 and udp", "acl 3 permit"], "Replacing a rule preserves its AND entries");
const reordered = plain(context.mergeConf([
  "acl 2 permit", "hostname example", "acl 1 drop"
], ["acl 2 and udp"]));
assert.deepEqual(reordered.filter(line => /^acl \d+ /.test(line)), ["acl 1 drop", "acl 2 and udp"], "Save places the predecessor before a converted AND entry");
assert.equal(reordered[1], "hostname example", "ACL ordering preserves other configuration positions");
assert.deepEqual(plain(context.mergeConf([
  "acl counter 0 width 32", "acl counter 1 width 64", "acl counter 0 mode bytes"
], ["acl counter 0 width 32"])).sort(), [
  "acl counter 0 mode bytes", "acl counter 0 width 32"
].sort(), "Pair settings replace both aliases without affecting mode");
assert.deepEqual(plain(context.mergeConf([
  "acl counter 4 mode packets", "acl counter 5 mode bytes", "acl counter 6 mode packets"
], ["acl counter 5 mode packets"])).sort(), [
  "acl counter 6 mode packets", "acl counter 4 mode packets"
].sort(), "Counter pair aliases normalize to the even counter");

assert.deepEqual(plain(context.mergeConf([
  "hostname switch", "acl counter 4 width 32", "acl counter 5 mode bytes", "vlan 10 1 2"
], ["acl counter 5 width 64"])), [
  "hostname switch", "acl counter 4 width 64", "acl counter 4 mode bytes", "vlan 10 1 2"
], "Counter settings normalize both aliases and use CONF_OVERWRITE to preserve their position");

(async () => {
  context.S.dirty = false;
  context.api = () => Promise.resolve({ok: false, status: 400, body: "acl: no free range entry\nError: acl ..."});
  await assert.rejects(context.postCmd("acl 1 drop", true), /acl: no free range entry/);
  assert.equal(context.S.dirty, false, "Rejected ACL command does not mark configuration dirty");
  context.api = () => Promise.resolve({ok: true, status: 200, body: "OK\n"});
  await context.postCmd("acl 1 drop", true);
  assert.equal(context.S.dirty, true, "Successful ACL command participates in Save to flash");
  await require("./test_acl_refresh.js")(app);
  console.log("ACL GUI: " + fixtureCount + " measured rule roundtrips, every match/action, validation, persistence and targeted card refreshes passed");
})().catch(error => { console.error(error); process.exitCode = 1; });

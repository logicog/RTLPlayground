/* Exercise real ACL card handlers and loaders without a switch or browser.
 * The fake DOM only hosts controls; request recording catches accidental full
 * tab reloads even when all 64 rule slots are occupied. */
"use strict";
const assert = require("node:assert/strict");
const vm = require("node:vm");

module.exports = async function testAclRefresh(app) {
  const elements = new Map();
  class Element {
    constructor(tag = "div") {
      this.tagName = tag;
      this.children = [];
      this.listeners = {};
      this.value = "";
      this.checked = false;
      this.textContent = "";
    }
    set innerHTML(value) { this.children = []; }
    setAttribute(key, value) {
      this[key] = String(value);
      if (key === "id") elements.set(this.id, this);
    }
    appendChild(child) {
      this.children.push(child);
      if (this.tagName === "select" && this.children.length === 1) this.value = child.value;
      return child;
    }
    addEventListener(type, listener) { (this.listeners[type] ||= []).push(listener); }
    dispatchEvent(event) {
      for (const listener of this.listeners[event.type] || []) listener.call(this, event);
    }
    querySelectorAll(selector) {
      const matches = node => selector === "input:checked" ? node.tagName === "input" && node.checked : node.tagName === selector;
      return this.children.flatMap(child => [...(matches(child) ? [child] : []), ...child.querySelectorAll(selector)]);
    }
    querySelector(selector) { return this.querySelectorAll(selector)[0]; }
  }
  const $ = id => {
    if (!elements.has(id)) { const node = new Element(); node.id = id; elements.set(id, node); }
    return elements.get(id);
  };
  function h(tag, attrs, kids) {
    const node = new Element(tag);
    for (const [key, value] of Object.entries(attrs || {})) {
      if (key.startsWith("on")) node.addEventListener(key.slice(2), value);
      else node.setAttribute(key, value);
    }
    for (const child of kids || []) node.appendChild(child);
    return node;
  }
  const reads = [], commands = [];
  const globalData = {drop: 0, gpio: 0, polarity: 1, counterMode: 1, counterWidth: 1, hit: ["00000000", "00000000"], exceeded: ["00000000", "00000000"]};
  const fieldData = Array.from({length: 16}, (_, n) => ({format: n % 8, offset: n * 2}));
  const counterData = Array(32).fill("00000000");
  const meter = n => ({n, rate: n * 100, burst: 4096, pps: n % 2, ifg: 1});
  let allowRuleRefresh = false;
  const ui = vm.createContext({
    S: {n: 9, ports: Array.from({length: 9}, (_, i) => ({portNum: i + 1, logPort: i}))},
    $, h, portLabel: p => p.portNum, t: key => key, toast: () => {},
    Event: class { constructor(type) { this.type = type; } },
    tabHooks: {}, needPorts: () => {},
    document: {querySelectorAll: () => []},
    confirmModal: (title, detail, ok) => ok(),
    postCmd: command => { commands.push(command); return Promise.resolve({ok: true}); },
    getJSON: url => {
      reads.push(url);
      if (url === "/acl.json") return Promise.resolve(globalData);
      if (url === "/aclfield.json") return Promise.resolve(fieldData);
      if (url === "/aclcounter.json") return Promise.resolve(counterData);
      const match = url.match(/^\/aclmeter\.json\?idx=(\d+)$/);
      if (match) {
        const start = +match[1], end = Math.min(64, start + 8);
        return Promise.resolve({meters: Array.from({length: end - start}, (_, n) => meter(start + n)), next: end < 64 ? end : 0});
      }
      if (allowRuleRefresh) {
        const ruleMatch = url.match(/^\/aclrule\.json\?idx=(\d+)$/);
        if (ruleMatch) {
          const start = +ruleMatch[1];
          return Promise.resolve({rules: Array.from({length: Math.min(4, 65 - start)}, (_, n) => ({n: start + n})), next: start < 61 ? start + 4 : 0});
        }
        if (/^\/aclrange\.json\?kind=[012]$/.test(url)) return Promise.resolve(Array(16).fill({type: 0, lo: "00000000", hi: "00000000"}));
      }
      return Promise.reject(new Error("Unexpected ACL refresh: " + url));
    }
  });
  const helperStart = app.indexOf("/* ACL helpers */"), uiStart = app.indexOf("/* ACL UI */"), uiEnd = app.indexOf("var IPRE", uiStart);
  assert(helperStart >= 0 && uiStart > helperStart && uiEnd > uiStart);
  vm.runInContext(app.slice(helperStart, uiEnd), ui);
  // Rendering is unrelated to request selection, and deliberately uses no DOM
  // shortcut for handlers or loaders: those run directly from app.js.
  ui.aclRenderRules = ui.aclRenderCounters = ui.aclRenderGlobals = () => {};
  ui.aclMode = () => {};
  ui.S.acl.rules = Array.from({length: 64}, (_, n) => ({n: n + 1}));
  ui.S.acl.ranges = Array.from({length: 3}, () => Array(16).fill({type: 0, lo: "00000000", hi: "00000000"}));
  ui.S.acl.global = globalData;
  ui.S.acl.meters = Array.from({length: 64}, (_, n) => ({n, rate: 10, burst: 20, pps: 0, ifg: 0}));
  ui.S.acl.fields = fieldData;
  ui.S.acl.counters = counterData;
  ui.S.acl.loaded = true;
  ui.aclSettings();
  const cachedRules = ui.S.acl.rules, cachedRanges = ui.S.acl.ranges;

  function clickCard(id) { $(id).querySelector("button").dispatchEvent({type: "click"}); }
  async function settled() {
    for (let n = 0; n < 200 && ui.S.acl.busy; n++) await Promise.resolve();
    assert.equal(ui.S.acl.busy, false, "ACL command completed");
    assert.equal($("acl-error").textContent, "", "Targeted refresh succeeded");
  }
  async function check(id, expectedCommands, expectedReads) {
    commands.length = 0; reads.length = 0;
    if (id.endsWith("-form")) clickCard(id); else $(id).dispatchEvent({type: "click"});
    await settled();
    assert.deepEqual(commands, expectedCommands, id + " posts the requested settings");
    assert.deepEqual(reads.slice().sort(), expectedReads.slice().sort(), id + " reloads only its own state");
    assert.equal(ui.S.acl.rules, cachedRules, id + " keeps the full cached rule table");
    assert.equal(ui.S.acl.ranges, cachedRanges, id + " keeps cached range state");
  }

  $("acl-meter-id").value = "37"; $("acl-rate").value = "1234"; $("acl-burst").value = "5678"; $("acl-units").value = "pps"; $("acl-ifg").checked = true;
  const otherMeter = ui.S.acl.meters[0];
  await check("acl-meter-form", ["acl meter 37 1234 pps 5678 ifg"], ["/aclmeter.json?idx=37"]);
  assert.equal(ui.S.acl.meters[37].rate, 3700, "Meter state is read back from its one response");
  assert.equal(ui.S.acl.meters[0], otherMeter, "Unrelated meter state stays cached");

  $("acl-field-id").value = "15"; $("acl-format").value = "l4"; $("acl-offset").value = "255";
  await check("acl-field-form", ["acl field 15 l4 255"], ["/aclfield.json"]);
  assert.equal(ui.S.acl.fields, fieldData, "Selector state is read back");

  $("acl-default").value = "permit";
  await check("acl-default-apply", ["acl default permit"], ["/acl.json"]);
  await check("acl-gpio-form", ["acl gpio 0 off", "acl gpio 1 off", "acl gpio 2 off", "acl gpio 3 off", "acl gpio polarity high"], ["/acl.json"]);

  $("acl-pair-id").value = "2"; $("acl-counter-mode").value = "bytes"; $("acl-counter-width").value = "64";
  await check("acl-counter-form", ["acl counter 2 mode bytes", "acl counter 2 width 64"], ["/acl.json", "/aclcounter.json"]);
  await check("acl-counter-reset", ["acl counter reset"], ["/aclcounter.json"]);

  allowRuleRefresh = true; reads.length = 0; commands.length = 0;
  await ui.aclRun("acl 1 drop");
  assert.deepEqual(commands, ["acl 1 drop"]);
  assert.equal(reads.filter(url => url.startsWith("/aclrule.json?")).length, 16, "A rule edit reloads all rule pages");
  assert.equal(reads.filter(url => url.startsWith("/aclrange.json?")).length, 3, "A rule edit reloads its range allocations");
  assert.equal(reads.length, 19, "A rule edit skips unrelated meter, selector and global endpoints");
  assert.equal($("acl-error").textContent, "");
};

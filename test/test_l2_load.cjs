const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../html/app.js'), 'utf8');
const start = source.indexOf('function l2Load() {');
const loader = source.slice(start, source.indexOf('\n}\n', start) + 2);

function fixture(getJSON) {
    const context = vm.createContext({
        S: { logToPhys: [1, 2, 3, 4, 5, 6, 7, 8] }, l2Gen: 1, l2Rows: [],
        l2Walk: { seen: {}, idx: 0, gen: 1, loading: false, done: false }, getJSON
    });
    vm.runInContext(loader, context);
    return context;
}

function row(idx) {
    return {
        idx: idx.toString(16).padStart(4, '0'), vlan: '001', type: 'l', port: 0, lag: 0,
        mac: '02:00:00:00:' + (idx >> 8).toString(16).padStart(2, '0') + ':' + (idx & 255).toString(16).padStart(2, '0')
    };
}

async function walk(capacity, pageSize) {
    const expected = Array.from({ length: capacity }, (_, i) => i);
    let requests = 0;
    const context = fixture(async url => {
        requests++;
        assert.ok(requests <= Math.ceil(capacity / pageSize) + 1, 'wrap or empty page ends the walk');
        let idx = Number(url.split('=')[1]);
        if (idx >= capacity) idx = 0;
        return expected.slice(idx, idx + pageSize).map(row);
    });
    await vm.runInContext('l2Load()', context);
    assert.equal(requests, 1, 'opening the table fetches one page, not the whole table');
    assert.deepEqual(Array.from(context.l2Rows, e => e.idx), expected.slice(0, pageSize));
    while (!context.l2Walk.done) await vm.runInContext('l2Load()', context);
    assert.deepEqual(Array.from(context.l2Rows, e => e.idx), expected, 'every address appears once');
    const finished = requests;
    await vm.runInContext('l2Load()', context);
    assert.equal(requests, finished, 'scrolling after completion makes no more requests');
}

(async () => {
    await walk(4096, 29);
    await walk(4096, 1);
    await walk(0, 29);

    let resolve, requests = 0;
    const context = fixture(() => {
        requests++;
        return new Promise(r => { resolve = r; });
    });
    const pending = vm.runInContext('l2Load()', context);
    await vm.runInContext('l2Load()', context);
    assert.equal(requests, 1, 'repeated scroll events cannot overlap page requests');
    context.l2Gen++;
    resolve([row(42)]);
    await assert.rejects(pending, /stale/);
    assert.deepEqual(Array.from(context.l2Rows), [], 'leaving the tab discards its in-flight page');

    const status = { textContent: 'loading...' }, more = { disabled: true };
    const staleError = fixture(() => new Promise((resolve, reject) => { staleError.reject = reject; }));
    staleError.$ = id => id === 'l2count' ? status : more;
    staleError.t = key => key;
    staleError.l2Render = () => {};
    const moreStart = source.indexOf('function l2More() {');
    vm.runInContext(source.slice(moreStart, source.indexOf('\n}\n', moreStart) + 2), staleError);
    const failedPage = vm.runInContext('l2More()', staleError);
    staleError.l2Gen++;
    status.textContent = 'new refresh';
    staleError.reject(new Error('network'));
    await failedPage;
    assert.equal(status.textContent, 'new refresh', 'an old request failure cannot overwrite a newer refresh');

    let fail = true;
    const retry = fixture(async () => {
        if (fail) throw new Error('network');
        return [row(17), row(99)];
    });
    await assert.rejects(vm.runInContext('l2Load()', retry), /network/);
    fail = false;
    await vm.runInContext('l2Load()', retry);
    assert.deepEqual(Array.from(retry.l2Rows, e => e.idx), [17, 99]);
    assert.equal(retry.l2Walk.idx, 100, 'sparse table cursors follow the last hardware index');
    console.log('PASS L2 loading: incremental pages, full tables, wrap, empty, overlap, cancellation and retry');
})().catch(error => { console.error(error); process.exitCode = 1; });

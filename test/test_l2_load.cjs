const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../html/app.js'), 'utf8');
const start = source.indexOf('function l2Load() {');
const loader = source.slice(start, source.indexOf('\n}\n', start) + 2);

async function walk(capacity, pageSize) {
    const expected = Array.from({ length: capacity }, (_, i) => i);
    const rows = expected.map(i => ({
        idx: i.toString(16).padStart(4, '0'), vlan: '001', type: 'l', port: i % 8, lag: 0,
        mac: '02:00:00:00:' + (i >> 8).toString(16).padStart(2, '0') + ':' + (i & 255).toString(16).padStart(2, '0')
    }));
    let requests = 0;
    const context = vm.createContext({
        S: { logToPhys: [1, 2, 3, 4, 5, 6, 7, 8] }, l2Gen: 0,
        getJSON: async url => {
            requests++;
            assert.ok(requests <= Math.ceil(capacity / pageSize) + 1, 'an empty page or a repeated index terminates the walk');
            let idx = Number(url.split('=')[1]);
            if (idx >= capacity) idx = 0;
            return structuredClone(rows.slice(idx, idx + pageSize));
        }
    });
    vm.runInContext(loader, context);
    const result = await vm.runInContext('l2Load()', context);
    assert.deepEqual(Array.from(result, e => e.idx), expected, `${capacity} entries, ${pageSize} per page: every address appears once`);
}

(async () => {
    await walk(4096, 29);
    await walk(4096, 1);
    await walk(0, 29);
    console.log('PASS L2 loading: complete 4096-entry tables, one-row pages, wrap and empty table');
})().catch(error => { console.error(error); process.exitCode = 1; });

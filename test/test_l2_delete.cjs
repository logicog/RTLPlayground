const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../html/app.js'), 'utf8');
const start = source.indexOf('function l2Render() {');
const render = source.slice(start, source.indexOf('\n}\n', start) + 2);

async function remaining(response) {
    const buttons = [];
    let reloads = 0;
    const tbody = {
        innerHTML: '',
        insertRow() {
            const row = { cells: [], insertCell() {
                const cell = { appendChild(button) { buttons.push(button); } };
                row.cells.push(cell);
                return cell;
            } };
            return row;
        }
    };
    const table = { tBodies: [tbody], querySelectorAll() { return []; } };
    const context = vm.createContext({
        l2Rows: [
            { idx: 17, pport: '3', where: '3', stat: false, mac: '02:11:22:33:44:55', vlan: 1 },
            { idx: 23, pport: '4', where: '4', stat: false, mac: '02:aa:bb:cc:dd:ee', vlan: 1 }
        ],
        l2SortCol: 'pport', l2SortDir: 1, l2Key: e => e.pport,
        t: text => text,
        $: id => id === 'l2table' ? table : id === 'l2filter' ? { value: '' } : {},
        h: (_, options) => options,
        l2Fetch: () => { reloads++; },
        getJSON: async () => { if (response instanceof Error) throw response; return response; }
    });
    vm.runInContext(render, context);
    vm.runInContext('l2Render()', context);
    buttons[0].onclick();
    await new Promise(resolve => setImmediate(resolve));
    return { rows: Array.from(context.l2Rows, e => e.idx), reloads };
}

(async () => {
    assert.deepEqual(await remaining({ result: 0 }), { rows: [17, 23], reloads: 1 },
        'a rejected deletion reloads the table instead of removing the row');
    assert.deepEqual(await remaining({ result: 1 }), { rows: [23], reloads: 0 },
        'a successful deletion removes only its own entry');
    assert.deepEqual(await remaining(new Error('network failure')), { rows: [17, 23], reloads: 0 },
        'a failed request keeps both entries visible');
    console.log('PASS L2 deletion: rejection reloads the table; success removes only its entry; failure keeps rows');
})().catch(error => { console.error(error); process.exitCode = 1; });

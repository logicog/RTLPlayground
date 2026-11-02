/* Real browser regression tests against the shipped HTML and JavaScript.
 * Network fixtures model both 6-port RTL8372 boards; no switch is contacted. */
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const root = path.resolve(__dirname, '..');
let checks = 0;
function check(value, detail) { assert.ok(value, detail); checks++; console.log('PASS ' + detail); }

(async () => {
  const launchOptions = { headless: true };
  const edge = 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
  if (process.env.BROWSER_EXE) launchOptions.executablePath = process.env.BROWSER_EXE;
  else if (process.platform === 'win32' && fs.existsSync(edge)) launchOptions.executablePath = edge;
  const browser = await chromium.launch(launchOptions);
  try {
    for (const [board, bytes] of [['LIANGUO LG-SG4T2', 0x80000], ['MIMC D4S2TXP', 0x200000]]) {
      const context = await browser.newContext({ locale: 'en-US', viewport: { width: 1440, height: 1000 } });
      const page = await context.newPage();
      const errors = [];
      page.on('pageerror', e => errors.push(e.message));
      const posts = [];
      let config = 'hostname test\nip 192.168.10.247\n';
      let log = 'sfp 1 10g\nstp port 1 p2p on\n';
      let failConfig = false, failLog = false, failClear = false, failVlan = false, rejectCmd = false;
      let vlans = [{ id: 1, name: '' }, { id: 2, name: 'test' }];
      let cable = {state:'idle',port:0,supported:[1,2,3,4],pairs:[]}, cablePolls=0;
      const ports = [4, 5, 6, 7, 8, 3].map((logPort, i) => ({ portNum: i + 1, logPort, name: i === 0 ? 'lan' : '', isSFP: i >= 4, enabled: 1, link: i >= 4 ? 5 : 6, adv: '0x3f', txG: '0x10', rxG: '0x10', txB: '0x0', rxB: '0x0', macs: '0x0', sfp_state: 0, sfp_options: 0 }));
      await context.route('http://rtl.test/**', async route => {
        const req = route.request(), u = new URL(req.url());
        const api = u.pathname;
        let body = '', status = 200, contentType = 'text/plain; charset=utf-8';
        if (req.method() === 'POST') {
          posts.push({ path: api, body: req.postData() });
          if (api === '/config') {
            const raw = req.postData();
            config = raw.split('\r\n\r\n')[1].split('\r\n--')[0];
          } else if (api === '/cmd' && rejectCmd) { status = 400; body = 'Error: rejected'; }
          else if(api === '/cmd' && req.postData()==='port 1 cabletest') {cable={state:'running',port:1,supported:[1,2,3,4],pairs:[]};cablePolls=0;}
        } else if (['/', '/index.html', '/app.js', '/login.html', '/favicon.ico'].includes(api)) {
          const file = api === '/' ? 'index.html' : api.slice(1);
          body = fs.readFileSync(path.join(root, 'html', file));
          contentType = file.endsWith('.js') ? 'application/javascript' : file.endsWith('.ico') ? 'image/x-icon' : 'text/html; charset=utf-8';
        } else if (api === '/information.json') {
          body = JSON.stringify({ hw_ver: board, sw_ver: 'audit-test', flash_size: bytes === 0x80000 ? '512 KB' : '2 MB', web_update: bytes >= 0x100000 ? 1 : 0, config_max: 4095, ip_address: '192.168.10.247', ip_gateway: '192.168.10.1', ip_netmask: '255.255.255.0', hostname: 'test' });
        } else if (api === '/status.json') body = JSON.stringify(ports);
        else if (api === '/mtu.json') body = JSON.stringify(ports.map(p => ({ portNum: p.portNum, mtu: '0x5ee' })));
        else if (api === '/config') { body = config; if (failConfig) status = 500; }
        else if (api === '/cmd_log') { body = log; if (failLog) status = 500; }
        else if (api === '/cmd_log_clear') { if (failClear) status = 500; else log = ''; }
        else if (api === '/vlanlist') body = JSON.stringify({ mgmt: 1, vlan: vlans });
        else if (api === '/vlan.json') { body = JSON.stringify({ name: vlans.find(v=>v.id===Number(u.searchParams.get('vid')))?.name || '', members: ((1<<25)|(0x3ff<<10)|(u.searchParams.get('vid')==='1'?0x2f8:0x100)).toString(16), pvid: u.searchParams.get('vid')==='1'?'0x00f8':'0x0100' }); if (failVlan) status = 500; }
        else if(api === '/cabletest.json') {
          if(cable.state==='running'&&++cablePolls>1)cable={state:'done',port:1,supported:[1,2,3,4],pairs:[{pair:'A',status:1,distance_dm:null},{pair:'B',status:2,distance_dm:123},{pair:'C',status:3,distance_dm:51},{pair:'D',status:0,distance_dm:null}]};
          body=JSON.stringify(cable);
        }
        else body = '[]';
        await route.fulfill({ status, contentType, body });
      });
      await page.goto('http://rtl.test/');
      await page.waitForFunction(() => S.n === 6 && S.info.hw_ver);
      await page.evaluate(() => statusPoller.stop());
      const flush = async () => { await page.evaluate(async () => {
        let stable = 0;
        for (let n = 0; n < 20 && stable < 2; n++) {
          const previous = _q;
          await previous;
          await new Promise(resolve => setTimeout(resolve, 20));
          stable = previous === _q ? stable + 1 : 0;
        }
      }); };
      check(await page.evaluate(() => rtlLang === 'zh' && document.documentElement.lang === 'zh-CN'), board + ': Chinese default even on English PC');
      check(JSON.stringify(await page.evaluate(() => ['000','001','010','100','011','101','110','111'].map(eeeFlags))) === JSON.stringify(['-','100M','1G','2.5G','100M, 1G','100M, 2.5G','1G, 2.5G','100M, 1G, 2.5G']), 'EEE speed labels match the actual 2.5G/1G/100M JSON bit order');
      check(await page.evaluate(() => Object.keys(LANG.en).filter(k => k !== 'v_duplicate_legacy').every(k => typeof LANG.zh[k] === 'string')), 'all English keys have a Chinese translation');
      check(await page.evaluate(() => isConfCmd('sfp 1 10g') && isConfCmd('stp port 1 p2p on') && isConfCmd('port 1 name') && isConfCmd('hostname test\nstp off')), 'persistent SFP, STP, cleared names and multi-line commands recognised');
      await page.evaluate(async () => {
        S.prev[0].txG = S.prev[0].rxG = '0xffff'; S.prevT = Date.now() - 1000;
        await pollStatus();
      });
      check(await page.evaluate(() => S.rates[0].tx === 0 && S.rates[0].rx === 0), 'counter resets do not display negative packet rates');
      const merged = await page.evaluate(() => mergeConf(['sfp 1 1g', 'stp pathcost short', 'stp bpdu flood'], ['sfp 1 10g\nstp pathcost long\nstp bpdu filter']));
      check(JSON.stringify(merged) === JSON.stringify(['sfp 1 10g', 'stp pathcost long', 'stp bpdu filter']), 'new persistent settings replace old values');
      await page.evaluate(() => showTab('ports'));
      await page.waitForFunction(() => document.getElementById('pmtu0').value);
      await flush();
      check(await page.locator('#ptable tbody tr').count() === 6, 'all four copper and both SFP ports visible');
      check(await page.locator('#pen4').isDisabled() && await page.locator('#pen5').isDisabled(), 'unsupported SFP enable switches disabled');
      await page.selectOption('#pspd0', '1g');
      await page.locator('#pname0').focus();
      await page.evaluate(() => portsStatus());
      check(await page.locator('#pspd0').inputValue() === '1g', 'polling preserves edited speed after focus changes');
      const before = posts.length;
      await page.fill('#pmtu0', ''); await page.evaluate(() => applyPort(0)); await flush();
      check(posts.length === before, 'blank MTU rejected without sending commands');
      await page.fill('#pmtu0', '1518'); await page.fill('#pname0', '中文'); await page.evaluate(() => applyPort(0)); await flush();
      check(posts.length === before, 'lossy Chinese port names rejected');
      await page.fill('#pname0', ''); await page.evaluate(() => applyPort(0)); await flush();
      check(posts.some(p => p.body === 'port 1 name '), 'clearing a port name sends the clear command');
      await page.evaluate(() => { $('pmtu4').value = 1518; $('pspd4').value = '10g'; applyPort(4); }); await flush();
      check(posts.some(p => p.body === 'sfp 1 10g') && !posts.some(p => /port [56] (on|off)/.test(p.body)), 'SFP applies only supported rate command');

      await page.evaluate(() => showTab('vlan')); await flush();
      await page.waitForFunction(() => $('vtable').tBodies[0].dataset.complete === '1');
      check(await page.locator('#vedit tbody tr').count()===6,'VLAN editor uses one clearly labelled row per physical port');
      check(await page.evaluate(()=>validVlanName('办公网络')&&validVlanName('访客_WiFi-2')&&isConfCmd('vlan 20 办公网络 1t')),'Chinese VLAN names accepted and recognised as persistent configuration');
      for(const bad of ['一'.repeat(16),'with space','bad"name','\ud800','mgmt','d'])check(!await page.evaluate(n=>validVlanName(n),bad),'unsafe or overlong VLAN name rejected: '+JSON.stringify(bad));
      check(await page.evaluate(()=>vlan2str(20,'',[],[1])==='vlan 20 - 1'),'clearing a VLAN name has an explicit persistent command');
      vlans[1].name='访客网络';await page.evaluate(()=>vlanRefresh());await flush();
      check((await page.locator('#vtable').textContent()).includes('访客网络'),'Chinese VLAN names displayed as text in the overview');
      check((await page.locator('#vmgmtcur').textContent()).includes('VLAN 1'),'management VLAN is visible without opening an editor');
      await page.evaluate(()=>vlanLoad(2));await flush();
      await page.evaluate(()=>{segSet(1,1);vlanPvidPreview();});
      check((await page.locator('#vp1').textContent()).includes('1 → 2'),'VLAN editor previews PVID changes before applying');
      await page.evaluate(()=>$('toasts').replaceChildren());
      await page.screenshot({path:path.join(root,'audit',bytes===0x80000?'vlan-preview-lianguo.png':'vlan-preview-mimc.png'),fullPage:true});
      await page.setViewportSize({width:390,height:844});
      await page.screenshot({path:path.join(root,'audit',bytes===0x80000?'vlan-preview-lianguo-mobile.png':'vlan-preview-mimc-mobile.png'),fullPage:true});
      await page.locator('#vedit').scrollIntoViewIfNeeded();
      await page.screenshot({path:path.join(root,'audit',bytes===0x80000?'vlan-editor-lianguo-mobile.png':'vlan-editor-mimc-mobile.png'),fullPage:true});
      await page.setViewportSize({width:1440,height:1000});
      await page.evaluate(() => $('vlan_add').click());
      for (const bad of ['0', '4095', '1.5', '']) {
        await page.fill('#nvlid', bad); const n = posts.length;
        await page.evaluate(() => addVlan()); await flush();
        check(posts.length === n, 'invalid VLAN ID rejected: ' + JSON.stringify(bad));
      }
      await page.fill('#nvlid', '1'); await page.evaluate(() => addVlan());
      check((await page.locator('#toasts').textContent()).includes('已存在'), 'duplicate VLAN has a distinct message');
      await page.evaluate(() => closeModal());
      await page.evaluate(() => vlanLoad(1)); await flush();
      const beforeVlan = posts.length;
      await page.evaluate(() => vlanApply());
      check(await page.locator('#mback').evaluate(e => e.classList.contains('show')) && posts.length === beforeVlan, 'management VLAN waits for explicit confirmation');
      await page.locator('#mfoot .pri').click(); await flush();
      check(posts.slice(beforeVlan).some(p => p.path === '/cmd' && p.body.includes('\npvid ') && p.body.endsWith('vlan 1 mgmt')), 'VLAN and PVID changes sent together with management last');
      failVlan = true; await page.evaluate(() => vlanRefresh()); await flush();
      const failedVlanPosts = posts.length;
      await page.locator('#vtable tbody tr').first().locator('button').first().click();
      await page.evaluate(() => vlanApply()); await flush();
      await page.evaluate(()=>addVlan());await page.click('#vlan_add');await flush();
      check(posts.length === failedVlanPosts && !(await page.locator('#mback').evaluate(e=>e.classList.contains('show'))), 'incomplete VLAN snapshot blocks applies, renames and creates');
      failVlan = false; vlans = []; await page.evaluate(async () => { await vlanRefresh(); VlanInit(); });
      check(await page.evaluate(() => !$('vtitle').dataset.id), 'empty VLAN list does not crash or retain a stale selection');

      for (const mode of ['config', 'log']) {
        failConfig = mode === 'config'; failLog = mode === 'log';
        const n = posts.filter(p => p.path === '/config').length;
        await page.evaluate(() => $('saveBtn').click()); await flush();
        check(posts.filter(p => p.path === '/config').length === n && !(await page.locator('#mback').evaluate(e => e.classList.contains('show'))), 'save aborts on failed ' + mode + ' read');
      }
      failConfig = false; failLog = false;
      await page.evaluate(() => { setDirty(true); $('saveBtn').click(); }); await flush();
      check(await page.locator('#mback').evaluate(e => e.classList.contains('show')), 'successful reads produce a reviewable save dialog');
      await page.locator('#mfoot .pri').click(); await flush();
      check(await page.evaluate(() => !S.dirty), 'successful save verifies readback and clears dirty state');
      failClear = true;
      await page.evaluate(() => { setDirty(true); doWriteConfig('hostname test\n', true); }); await flush();
      check(await page.evaluate(() => S.dirty) && (await page.locator('#toasts').textContent()).includes('日志未能清除'), 'failed log clearing retains dirty state and reports the problem');
      failClear = false;
      await page.evaluate(() => writeConfig('#' + 'x'.repeat(124) + '\n'.repeat(1) + ('#' + 'x'.repeat(124) + '\n').repeat(31), 'limit'));
      check(await page.locator('#mfoot .pri').isEnabled(), 'configuration beyond former 2048-byte limit can be saved');
      await page.locator('#mbody textarea').fill('x'.repeat(4096));
      check(await page.locator('#mfoot .pri').isDisabled(), '4096-byte configuration blocked');
      await page.locator('#mbody textarea').fill('x'.repeat(127));
      check(await page.locator('#mfoot .pri').isDisabled(), 'overlong startup command blocked');
      await page.evaluate(() => closeModal());
      await page.evaluate(() => { $('sy-pw1').value = $('sy-pw2').value = '中文密码'; $('sy-pwapply').click(); }); await flush();
      check(!(await page.locator('#mback').evaluate(e => e.classList.contains('show'))), 'unrepresentable password rejected before confirmation');
      const invalidNetPosts = posts.length;
      await page.evaluate(() => {
        $('sy-ip').value = '192.168.10.247'; $('sy-mask').value = '255.0.255.0'; $('sy-gw').value = '192.168.10.1';
        $('sy-apply').click();
      }); await flush();
      check(posts.length === invalidNetPosts && !(await page.locator('#mback').evaluate(e => e.classList.contains('show'))), 'non-contiguous subnet masks are rejected');
      rejectCmd = true;
      await page.evaluate(() => postCmd('stp off').catch(() => {})); await flush();
      check((await page.locator('#toasts').textContent()).includes('Error: rejected'), 'HTTP command errors remain visible');
      rejectCmd = false;

      await page.evaluate(()=>showTab('cable'));await flush();
      check(await page.locator('#ct-port option').count()===4,'cable diagnostics offers copper ports only');
      check(!posts.some(p=>p.body==='port 1 cabletest'),'opening cable diagnostics never starts a disruptive test');
      await page.selectOption('#ct-port','1');await page.click('#ct-start');
      check(await page.locator('#mback').evaluate(e=>e.classList.contains('show'))&&!posts.some(p=>p.body==='port 1 cabletest'),'cable test waits for explicit link-interruption confirmation');
      await page.locator('#mfoot .pri').click();
      await page.waitForFunction(()=>$('ct-state').textContent==='检测完成');
      check(await page.locator('#ct-results tr').count()===4&&(await page.locator('#ct-results').textContent()).includes('12.3 m'),'four wire pairs and fault distance render after asynchronous completion');
      check((await page.locator('#ct-results').textContent()).includes('不提供距离'),'normal or unknown pairs do not display an invented cable length');
      cable={state:'timeout',port:1,supported:[1,2,3,4],pairs:[]};await page.evaluate(()=>cableLoad());
      check((await page.locator('#ct-state').textContent()).includes('超时')&&!(await page.locator('#ct-start').isDisabled()),'timeout restores the test button and gives a distinct status');

      await page.evaluate(() => { showTab('fw'); fwCapacity(); });
      check(await page.locator('#fwfile').isDisabled() === (bytes < 0x100000), 'upload gated by actual Flash capacity');
      check(await page.locator('#fwup').isDisabled(), 'upload unavailable without a validated image');
      await page.evaluate(() => $('toasts').replaceChildren());
      await page.screenshot({ path: path.join(root, 'audit', bytes === 0x80000 ? 'lianguo-web-update.png' : 'mimc-web-update.png'), fullPage: true });
      await page.setViewportSize({ width: 390, height: 844 });
      await page.evaluate(() => showTab('ports'));
      await page.screenshot({ path: path.join(root, 'audit', bytes === 0x80000 ? 'lianguo-mobile.png' : 'mimc-mobile.png'), fullPage: true });
      check(errors.length === 0, 'no browser JavaScript errors: ' + errors.join('; '));
      await page.goto('http://rtl.test/login.html');
      check(await page.locator('#lb').textContent() === '登录' && await page.locator('html').getAttribute('lang') === 'zh-CN', 'login defaults to Chinese');
      await context.close();
    }
    console.log(`ALL ${checks} WEB UI CHECKS PASSED`);
  } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });

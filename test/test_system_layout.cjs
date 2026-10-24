const fs=require('node:fs'),assert=require('node:assert/strict');
const {chromium}=require('playwright');
const src=fs.readFileSync('html/app.js','utf8');
const html=fs.readFileSync(process.env.SYSTEM_HTML||'html/index.html','utf8').replace(/<script\b[^>]*>[\s\S]*?<\/script>/g,'');
const render=src.slice(src.indexOf('var snmpPending='),src.indexOf('function snmpManagerValue('));
(async()=>{const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});
try{const page=await browser.newPage();await page.setContent(html);
 await page.addScriptTag({content:'function $(id){return document.getElementById(id)}\n'+render});
 await page.evaluate(()=>{document.querySelectorAll('.tab').forEach(n=>n.classList.remove('act'));document.getElementById('tab-system').classList.add('act')});
 let count=0;
 for(const width of [320,360,640,760,800,1024,1280,1600]){await page.setViewportSize({width,height:1000});
 for(const state of ['enabled','disabled','unsupported']){
 const baselineWidth=await page.evaluate(()=>{renderSnmp(undefined);return document.documentElement.scrollWidth});
 await page.evaluate(state=>renderSnmp(state==='unsupported'?undefined:[state==='enabled'?1:0,1,192,0,2,250,255,255,255,255]),state);
 const d=await page.evaluate(()=>{const card=document.getElementById('sy-snmp');return {pageWidth:document.documentElement.scrollWidth,overflow:document.documentElement.scrollWidth>innerWidth,cardOverflow:card.scrollWidth>card.clientWidth,display:getComputedStyle(card).display}});
 assert(d.pageWidth<=baselineWidth,`${width}/${state}: SNMP adds overflow beyond upstream (${d.pageWidth}/${baselineWidth})`);
 if(state!=='unsupported'){assert.equal(d.cardOverflow,false,`${width}: card overflow`);assert.equal(await page.locator('#sy-snmp-toggle').isChecked(),state==='enabled');assert.equal(await page.locator('#sy-snmp-manager').inputValue(),'192.0.2.250/32')}
 else assert.equal(d.display,'none');
 assert.equal(await page.locator('#sy-cmd').isVisible(),true);count++;
 }}
 console.log(`PASS ${count} browser cases: SNMP on/off/absent, no additional upstream overflow, card bounds and CIDR rendering`);
}finally{await browser.close()}})().catch(e=>{console.error(e);process.exitCode=1});

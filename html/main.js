/** INFRASTRUCTURE **/
let toastTimer;
function notify(msg, type = 'info') {
  const t = document.getElementById('toast');
  clearTimeout(toastTimer); t.className = `show ${type}`; t.innerText = msg;
  toastTimer = setTimeout(() => { t.className = t.className.replace('show', '').trim(); }, 4000);
}

let isFlashing = false;
let reqQ = [], busy = false, poller = null, systemInterval = null, inFlight = null;
const linkS = ["Disabled", "Down", "10M", "100M", "1000M", "500M", "10G", "2.5G", "5G"];
let globalNumPorts = 0, physToLogMap = {}, sfpMap = {}, portNames = {};

const sysLabels = {
  "ip_address": "IP Address", "ip_netmask": "Subnet Mask", "ip_gateway": "Default Gateway",
  "sw_ver": "Firmware Version", "hw_ver": "Hardware Version", "mac_addr": "MAC Address", "uptime": "System Uptime"
};

function fetchAPI(method, url, cb, data = null) {
  if (isFlashing) return;
  if (inFlight && inFlight.url === url && inFlight.method === method) return;
  if (reqQ.some(r => r.url === url && r.method === method)) return;
  reqQ.push({ method, url, cb, data });
  if (!busy) processQ();
}

function processQ() {
  if (reqQ.length === 0 || isFlashing) { busy = false; return; }
  busy = true; const r = reqQ.shift(); inFlight = { url: r.url, method: r.method };
  const x = new XMLHttpRequest(); 
  
  let finalUrl = r.url;
  if (r.method === "GET") finalUrl += (finalUrl.includes('?') ? '&' : '?') + '_t=' + new Date().getTime();
  
  x.open(r.method, finalUrl, true); 
  x.timeout = 4000; 
  x.onreadystatechange = function() {
    if (this.readyState === 4) {
      inFlight = null; 
      if (isFlashing) return; 
      if (this.status === 200 && r.cb) r.cb(this.responseText);
      else if (this.status === 401) { document.location = '/login.html'; return; }
      setTimeout(processQ, 50);
    }
  };
  x.ontimeout = x.onerror = () => { inFlight = null; setTimeout(processQ, 100); };
  x.send(r.data);
}

/** SMART ROUTING (Live Polling vs One-Time Loads) **/
function nav(id) {
  if (isFlashing || document.getElementById('flashBtn').disabled) return notify("Locked during update.", "warning");
  
  clearInterval(poller); poller = null;
  clearInterval(systemInterval); systemInterval = null;
  
  document.querySelectorAll('.panel, #nav li').forEach(e => e.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  document.getElementById('nav-' + id).classList.add('active');
  
  if (id === 'dash') {
    pollStatus(); poller = setInterval(pollStatus, 5000);
    pollInfo(); systemInterval = setInterval(pollInfo, 5000);
  } 
  else if (id === 'port') {
    pollStatus(); poller = setInterval(pollStatus, 5000);
    loadPortConfig(); 
  } 
  else if (id === 'stat') {
    pollStatus(); poller = setInterval(pollStatus, 5000);
  } 
  else if (id === 'vlan') {
    initVlanTable(); 
  } 
  else if (id === 'l2') {
    loadL2Config();
  } 
  else if (id === 'sys') {
    pollInfo(); systemInterval = setInterval(pollInfo, 5000);
    loadSysConfig();
  }
}

/** LIVE DATA POLLERS (Runs every 5s) **/
function pollStatus() {
  fetchAPI("GET", "/status.json", (raw) => {
    try {
      const data = JSON.parse(raw);
      const grid = document.getElementById('port-grid');
      const sBody = document.getElementById('stat-body');
      const isDash = document.getElementById('dash').classList.contains('active');
      const isStat = document.getElementById('stat').classList.contains('active');
      
      if (!globalNumPorts || globalNumPorts !== data.length) {
          globalNumPorts = data.length;
          grid.innerHTML = ''; sBody.innerHTML = '';
      }

      if (isDash && !grid.children.length && data.length > 0) {
        data.forEach(p => {
          if (isNaN(parseInt(p.portNum, 10))) return;
          const d = document.createElement('div');
          d.id = 'port-' + p.portNum; d.className = 'port'; grid.appendChild(d);
        });
      }

      if (isStat && (!sBody.children.length || sBody.children[0].cells.length < 8)) {
        sBody.innerHTML = '';
        data.forEach(p => {
          if (isNaN(parseInt(p.portNum, 10))) return;
          const tr = document.createElement('tr');
          tr.id = 'stat-' + p.portNum;
          for(let i=0; i<8; i++) tr.appendChild(document.createElement('td'));
          sBody.appendChild(tr);
        });
      }

      data.forEach(p => {
        const portNum = parseInt(p.portNum, 10);
        if (isNaN(portNum)) return; 
        physToLogMap[portNum] = p.logPort; 
        sfpMap[portNum] = p.isSFP;
        portNames[portNum] = p.name || ''; 
        
        const linkRaw = parseInt(p.link, 10);
        let linkText = "UNKNOWN"; let isUp = false;
        
        if (p.enabled == 0) linkText = "Disabled";
        else if (!isNaN(linkRaw)) { isUp = linkRaw > 0; linkText = linkS[linkRaw + 1] || "UNKNOWN"; }
        
        let tooltip = `Port ${portNum}`;
        if (portNames[portNum]) tooltip += ` (${portNames[portNum]})`;
        tooltip += `\nLink State: ${linkText}`;
        if (isUp) {
            tooltip += `\nTx Packets: ${p.txG ? BigInt(p.txG).toString() : "0"}`;
            tooltip += `\nRx Packets: ${p.rxG ? BigInt(p.rxG).toString() : "0"}`;
        }
        
        if (p.isSFP) {
            tooltip += `\n\n--- Transceiver Diagnostics ---`;
            if (p.sfp_vendor) tooltip += `\nVendor: ${p.sfp_vendor} (${p.sfp_model})`;
            
            if (p.sfp_temp && p.sfp_temp !== "0x0000") {
                let tRaw = parseInt(p.sfp_temp, 16);
                if (tRaw > 32767) tRaw -= 65536; 
                tooltip += `\nTemp: ${(tRaw / 256).toFixed(1)} °C`;
            }
            if (p.sfp_vcc && p.sfp_vcc !== "0x0000") {
                let vcc = parseInt(p.sfp_vcc, 16) * 0.0001;
                tooltip += `\nVoltage: ${vcc.toFixed(2)} V`;
            }
            if (p.sfp_txbias && p.sfp_txbias !== "0x0000") {
                let bias = parseInt(p.sfp_txbias, 16) * 0.002;
                tooltip += `\nTx Bias: ${bias.toFixed(2)} mA`;
            }
            if (p.sfp_txpower && p.sfp_txpower !== "0x0000") {
                let mw = parseInt(p.sfp_txpower, 16) * 0.0001;
                let dbm = mw > 0 ? (10 * Math.log10(mw)).toFixed(2) : "-inf";
                tooltip += `\nTx Power: ${dbm} dBm`;
            }
            if (p.sfp_rxpower && p.sfp_rxpower !== "0x0000") {
                let mw = parseInt(p.sfp_rxpower, 16) * 0.0001;
                let dbm = mw > 0 ? (10 * Math.log10(mw)).toFixed(2) : "-inf";
                tooltip += `\nRx Power: ${dbm} dBm`;
            } else if (p.sfp_rxpower === "0x0000") {
                tooltip += `\nRx Power: LOS (No light)`;
            }
        }
        
        if (isDash) {
          const d = document.getElementById('port-' + portNum);
          if (d) {
            d.className = isUp ? 'port up' : 'port'; d.title = tooltip; d.innerHTML = ''; 
            if (p.isSFP) {
              const badge = document.createElement('div');
              badge.className = 'sfp-badge'; badge.textContent = 'SFP';
              d.appendChild(badge); d.appendChild(document.createElement('br'));
            }
            d.appendChild(document.createTextNode(`PORT ${portNum}`));
            if (portNames[portNum]) {
               const nSpan = document.createElement('div');
               nSpan.style.fontSize = '11px'; nSpan.style.color = isUp ? '#065f46' : '#94a3b8';
               nSpan.style.marginTop = '2px'; nSpan.style.overflow = 'hidden'; 
               nSpan.style.textOverflow = 'ellipsis'; nSpan.style.whiteSpace = 'nowrap';
               nSpan.textContent = portNames[portNum]; d.appendChild(nSpan);
            }
            d.appendChild(document.createElement('br'));
            const span = document.createElement('span');
            span.style.fontSize = '16px'; span.textContent = linkText; d.appendChild(span);
          }
        }
        
        if (isStat) {
          const tr = document.getElementById('stat-' + portNum);
          if (tr) {
            tr.cells[0].innerHTML = `<strong>Port ${portNum}</strong>`;
            tr.cells[1].textContent = portNames[portNum]; tr.cells[2].textContent = linkText;
            tr.cells[3].textContent = p.txG ? BigInt(p.txG).toString() : "0"; tr.cells[3].style.color = 'var(--success)';
            tr.cells[4].textContent = p.txB ? BigInt(p.txB).toString() : "0"; tr.cells[4].style.color = 'var(--danger)';
            tr.cells[5].textContent = p.rxG ? BigInt(p.rxG).toString() : "0"; tr.cells[5].style.color = 'var(--success)';
            tr.cells[6].textContent = p.rxB ? BigInt(p.rxB).toString() : "0"; tr.cells[6].style.color = 'var(--danger)';
            tr.cells[7].innerHTML = `<button class="btn" onclick="showCounters(${portNum})" style="padding: 4px 8px; font-size: 11px;">Details</button>`;
          }
        }
      });
    } catch (e) { console.warn(e); }
  });
}

function pollInfo() {
  fetchAPI("GET", "/information.json", (raw) => {
    try {
      const data = JSON.parse(raw);
      const tbl = document.getElementById('info-table');
      if (tbl) {
        tbl.innerHTML = '';
        for (const [k, v] of Object.entries(data)) {
          const row = tbl.insertRow();
          const keyCell = row.insertCell();
          const strong = document.createElement('strong');
          strong.textContent = sysLabels[k] || k;
          keyCell.appendChild(strong);
          row.insertCell().textContent = v; 
        }
      }
      const sysIp = document.getElementById('sys-ip');
      if (sysIp && document.activeElement !== sysIp) {
        sysIp.value = data.ip_address || ''; document.getElementById('sys-mask').value = data.ip_netmask || '';
        document.getElementById('sys-gw').value = data.ip_gateway || '';
      }
    } catch (e) {}
  });
}


/** ONE-TIME STATIC DATA LOADERS (Runs once per tab switch) **/
function loadPortConfig() {
  let isGlobalEEE = false;
  // Get EEE config first
  fetchAPI("GET", "/config", (raw) => {
    const cleanRaw = raw.replace(/[^\x09\x0A\x0D\x20-\x7E]/g, '').trim();
    parseConf(cleanRaw);
    
    const eeeMatch = cleanRaw.match(/^eee\s+(on|off)/m);
    isGlobalEEE = (eeeMatch && eeeMatch[1] === 'on');
    const chk = document.getElementById('eee-en');
    if (chk) chk.checked = isGlobalEEE;

    // Then draw port table
    fetchAPI("GET", "/mtu.json", (mtuRaw) => {
      try {
        let tbody = document.getElementById('port-cfg-body');
        tbody.innerHTML = ''; 
        
        JSON.parse(mtuRaw).forEach(p => {
          const portNum = parseInt(p.portNum, 10);
          if(isNaN(portNum)) return;
          const mtuHex = parseInt(p.mtu, 16);
          const speedVal = p.speed ? p.speed : (p.enabled == 0 ? "off" : "auto"); 
          
          let eeeState = "off";
          const eeeRegex = new RegExp(`^eee\\s+${portNum}\\s+(on|off)`);
          configuration.forEach(line => { const m = line.match(eeeRegex); if (m) eeeState = m[1]; });
          
          let tr = document.createElement('tr');
          tr.id = 'pcfg-row-' + portNum;
          tr.innerHTML = `
            <td><strong>Port ${portNum}</strong></td>
            <td><input type="text" id="pname${portNum}" style="width:90px;" placeholder="Name" value="${portNames[portNum] || ''}"></td>
            <td><select id="speed${portNum}" style="width:120px;">
              <option value="auto" ${speedVal==='auto'?'selected':''}>Auto</option>
              <option value="off" ${speedVal==='off'?'selected':''}>Disabled</option>
              <option value="2g5" ${speedVal==='2g5'?'selected':''}>2.5G/Full</option>
              <option value="1g" ${speedVal==='1g'?'selected':''}>1G/Full</option>
              <option value="100m full" ${speedVal==='100m full'?'selected':''}>100M/Full</option>
              <option value="100m half" ${speedVal==='100m half'?'selected':''}>100M/Half</option>
              <option value="10m full" ${speedVal==='10m full'?'selected':''}>10M/Full</option>
              <option value="10m half" ${speedVal==='10m half'?'selected':''}>10M/Half</option>
            </select></td>
            <td><select id="mtu${portNum}" style="width:80px;">
              <option value="1522" ${mtuHex===1522?'selected':''}>1522</option>
              <option value="1536" ${mtuHex===1536?'selected':''}>1536</option>
              <option value="1552" ${mtuHex===1552?'selected':''}>1552</option>
              <option value="9216" ${mtuHex===9216?'selected':''}>9216</option>
              <option value="16383" ${mtuHex===16383?'selected':''}>16383</option>
            </select></td>
            <td><select id="eeeport${portNum}" style="width:70px;" ${isGlobalEEE ? 'disabled' : ''}>
              <option value="off" ${eeeState==='off'?'selected':''}>Off</option>
              <option value="on" ${eeeState==='on'?'selected':''}>On</option>
            </select></td>
            <td><button class="btn" id="pcfg_btn_${portNum}" style="padding: 4px 8px; font-size: 11px;" onclick="applyPortCfg(${portNum})">Save</button></td>
          `;
          tbody.appendChild(tr);
        });
      } catch(e) {}
    });
  });

  fetchAPI("GET", "/bandwidth.json", (raw) => {
    try {
      const s = JSON.parse(raw);
      const tbody = document.getElementById('bw-body');
      tbody.innerHTML = '';
      
      s.forEach(p => {
        const portNum = parseInt(p.portNum, 10);
        if (isNaN(portNum)) return;
        
        let iBW = parseInt(p.iBW, 16) * 16; 
        let eBW = parseInt(p.eBW, 16) * 16;
        
        let tr = document.createElement('tr');
        tr.id = 'bw-row-' + portNum;
        tr.innerHTML = `
          <td><strong>Port ${portNum}</strong></td>
          <td class="cb-cell"><input type="checkbox" id="ilim_${portNum}" ${p.iLimited?'checked':''} onchange="toggleBW(${portNum}, 'i')"></td>
          <td id="td_ibw_${portNum}"><input type="number" id="ibw_${portNum}" style="width:80px;" value="${p.iLimited?iBW:''}" placeholder="${p.iLimited?'':'UNLIMITED'}" ${p.iLimited?'':'disabled'} oninput="document.getElementById('bwbtn_${portNum}').disabled=false"></td>
          <td class="cb-cell"><input type="checkbox" id="ifc_${portNum}" ${p.iFC?'checked':''} ${p.iLimited?'':'disabled'} onchange="document.getElementById('bwbtn_${portNum}').disabled=false"></td>
          <td class="cb-cell"><input type="checkbox" id="elim_${portNum}" ${p.eLimited?'checked':''} onchange="toggleBW(${portNum}, 'e')"></td>
          <td id="td_ebw_${portNum}"><input type="number" id="ebw_${portNum}" style="width:80px;" value="${p.eLimited?eBW:''}" placeholder="${p.eLimited?'':'UNLIMITED'}" ${p.eLimited?'':'disabled'} oninput="document.getElementById('bwbtn_${portNum}').disabled=false"></td>
          <td><button class="btn" id="bwbtn_${portNum}" style="padding: 4px 8px; font-size: 11px;" onclick="applyBandwidth(${portNum})" disabled>Apply</button></td>
        `;
        tbody.appendChild(tr);
      });
    } catch(e) {}
  });
}

function loadSysConfig() {
  fetchAPI("GET", "/config", (raw) => {
    const cleanRaw = raw.replace(/[^\x09\x0A\x0D\x20-\x7E]/g, '').trim();
    parseConf(cleanRaw);
    document.getElementById('config-window').value = cleanRaw;
  });
}

function loadL2Config() {
  fetchAPI("GET", "/config", (raw) => {
    const cleanRaw = raw.replace(/[^\x09\x0A\x0D\x20-\x7E]/g, '').trim();
    parseConf(cleanRaw);
    const igmpMatch = cleanRaw.match(/^igmp\s+(on|off)/m);
    document.getElementById('igmp-en').checked = (igmpMatch && igmpMatch[1] === 'on');
  });

  fetchAPI("GET", "/lag.json", (raw) => {
    try {
      const s = JSON.parse(raw);
      let lagHtml = '';
      for (let l = 0; l < 4; l++) {
        let members = s[l] ? parseInt(s[l].members, 2) : 0;
        lagHtml += `<div class="lag-group"><h4>LAG Group ${l}</h4><div style="display:flex; flex-wrap:wrap; gap:10px; margin-bottom:10px;">`;
        for(let i=1; i<=globalNumPorts; i++) {
          const bit = physToLogMap[i] !== undefined ? physToLogMap[i] : (i - 1);
          const isChecked = (members & (1 << bit)) ? 'checked' : '';
          lagHtml += `<label style="font-size:13px; cursor:pointer;"><input type="checkbox" id="lag${l}_p${i}" ${isChecked} style="margin-right:4px;">P${i}</label>`;
        }
        lagHtml += `</div><button class="btn" style="padding: 6px 12px; font-size: 12px;" onclick="applyLAG(${l})">Save LAG ${l}</button></div>`;
      }
      document.getElementById('lag-container').innerHTML = lagHtml;
    } catch (e) {}
  });

  fetchAPI("GET", "/mirror.json", (raw) => {
    try {
      const s = JSON.parse(raw);
      document.getElementById('mirror-en').checked = s.enabled;
      document.getElementById('mirror-port').value = s.mPort;
      const m_tx = parseInt(s.mirror_tx, 16);
      const m_rx = parseInt(s.mirror_rx, 16);
      
      let rows = '';
      for (let i = 1; i <= globalNumPorts; i++) {
        const bit = physToLogMap[i] !== undefined ? physToLogMap[i] : (i - 1);
        rows += `<tr><td><strong>Port ${i}</strong></td><td class="cb-cell"><input type="checkbox" id="mtx${i}" ${(m_tx & (1<<bit)) ? 'checked' : ''}></td><td class="cb-cell"><input type="checkbox" id="mrx${i}" ${(m_rx & (1<<bit)) ? 'checked' : ''}></td></tr>`;
      }
      document.getElementById('mirror-body').innerHTML = rows;
    } catch (e) {}
  });
}

/** DATA SAVING COMMANDS **/
function applyPortCfg(p) {
  // Lock the button to prevent double-clicks or visual reverts
  const btn = document.getElementById(`pcfg_btn_${p}`);
  if (btn) btn.disabled = true;

  const speed = document.getElementById(`speed${p}`).value;
  const mtu = document.getElementById(`mtu${p}`).value;
  const eeeP = document.getElementById(`eeeport${p}`).value;
  
  const pname = document.getElementById(`pname${p}`).value.trim().replace(/\s+/g, '_');
  
  fetchAPI("POST", "/cmd", () => {}, pname ? `port ${p} name ${pname}` : `port ${p} name ""`);
  fetchAPI("POST", "/cmd", () => {}, `port ${p} ${speed}`);
  
  if (!document.getElementById('eee-en').checked) {
      fetchAPI("POST", "/cmd", () => {}, `eee ${p} ${eeeP}`);
  }
  
  // The final command unlocks the button on completion
  fetchAPI("POST", "/cmd", () => {
      notify(`Port ${p} configuration applied.`, "success");
      if (btn) btn.disabled = false;
  }, `mtu ${p} ${mtu}`);
}

function applyGlobalEEE() {
  const isEnabled = document.getElementById('eee-en').checked;
  fetchAPI("POST", "/cmd", () => notify(`Global EEE turned ${isEnabled ? 'ON' : 'OFF'}.`, "success"), isEnabled ? "eee on" : "eee off");
  
  for(let p=1; p<=globalNumPorts; p++) {
     let s = document.getElementById(`eeeport${p}`);
     if (s) s.disabled = isEnabled;
     if (isEnabled) {
         fetchAPI("POST", "/cmd", () => {}, `eee ${p} off`);
         if (s) s.value = 'off';
     }
  }
}

function toggleBW(p, dir) {
  document.getElementById(`bwbtn_${p}`).disabled = false;
  if (dir === 'i') {
    const lim = document.getElementById(`ilim_${p}`).checked;
    const inp = document.getElementById(`ibw_${p}`);
    inp.disabled = !lim;
    if (!lim) { inp.value = ""; inp.placeholder = "UNLIMITED"; } else if (!inp.value) { inp.value = "0"; }
    
    const ifc = document.getElementById(`ifc_${p}`);
    ifc.disabled = !lim;
    if (lim) ifc.checked = true;
  } else {
    const lim = document.getElementById(`elim_${p}`).checked;
    const inp = document.getElementById(`ebw_${p}`);
    inp.disabled = !lim;
    if (!lim) { inp.value = ""; inp.placeholder = "UNLIMITED"; } else if (!inp.value) { inp.value = "0"; }
  }
}

function applyBandwidth(p) {
  const logPort = p - 1; 
  
  if (document.getElementById(`ilim_${p}`).checked) {
    let userVal = parseInt(document.getElementById(`ibw_${p}`).value || 0);
    let hexVal = Math.floor(userVal / 16).toString(16).padStart(4, "0");
    fetchAPI("POST", "/cmd", () => {}, `bw in ${logPort} ${hexVal}`);
    if (!document.getElementById(`ifc_${p}`).checked) fetchAPI("POST", "/cmd", () => {}, `bw in ${logPort} drop`);
  } else {
    fetchAPI("POST", "/cmd", () => {}, `bw in ${logPort} off`);
  }
  
  if (document.getElementById(`elim_${p}`).checked) {
    let userVal = parseInt(document.getElementById(`ebw_${p}`).value || 0);
    let hexVal = Math.floor(userVal / 16).toString(16).padStart(4, "0");
    fetchAPI("POST", "/cmd", () => {}, `bw out ${logPort} ${hexVal}`);
  } else {
    fetchAPI("POST", "/cmd", () => {}, `bw out ${logPort} off`);
  }
  
  document.getElementById(`bwbtn_${p}`).disabled = true;
  notify(`Bandwidth limits applied for Port ${p}`, "success");
}

function initVlanTable() {
  const tbody = document.getElementById('vlan-body');
  const portCount = globalNumPorts || 10; 
  if (tbody.children.length === portCount && tbody.children[0].cells.length > 1) return;
  
  let rows = '';
  for (let i = 1; i <= portCount; i++) {
    rows += `<tr><td><strong>Port ${i}</strong></td><td class="cb-cell"><input type="checkbox" id="tport${i}" onclick="document.getElementById('uport${i}').checked=false"></td><td class="cb-cell"><input type="checkbox" id="uport${i}" onclick="document.getElementById('tport${i}').checked=false"></td><td class="cb-cell"><input type="checkbox" id="pport${i}"></td></tr>`;
  }
  tbody.innerHTML = rows;
}

function fetchVLAN() {
  const vid = document.getElementById('vlan-vid').value;
  if (!vid) return notify("Please enter a VLAN ID first.", "warning");
  
  fetchAPI("GET", `/vlan.json?vid=${vid}`, (raw) => {
    try {
      const s = JSON.parse(raw);
      document.getElementById('vlan-name').value = s.name || '';
      
      const members = parseInt(s.members, 16);
      const untag = s.untag !== undefined ? parseInt(s.untag, 16) : ((members >> 10) & 0x3FF);
      const pvidMask = parseInt(s.pvid, 16); 
      
      for (let p = 1; p <= globalNumPorts; p++) {
        const bit = physToLogMap[p] !== undefined ? physToLogMap[p] : (p - 1);
        document.getElementById(`tport${p}`).checked = ((members >> bit) & 1) && !((untag >> bit) & 1);
        document.getElementById(`uport${p}`).checked = ((members >> bit) & 1) && ((untag >> bit) & 1);
        document.getElementById(`pport${p}`).checked = (pvidMask >> bit) & 1;
      }
      notify(`Loaded VLAN ${vid} successfully.`, "success");
    } catch (e) { notify("VLAN not found.", "error"); }
  });
}

function applyVLAN() {
  const vid = document.getElementById('vlan-vid').value;
  if (!vid) return notify("Please enter a VLAN ID first.", "warning");
  
  const vlanName = document.getElementById('vlan-name').value.trim().replace(/\s+/g, '_');
  let allMembers = [];
  let untagged = [];
  
  for (let p = 1; p <= globalNumPorts; p++) {
    if (document.getElementById(`tport${p}`).checked) allMembers.push(p);
    if (document.getElementById(`uport${p}`).checked) { allMembers.push(p); untagged.push(p); }
  }
  
  let vlanCmd = `vlan ${vid}`;
  if (vlanName) vlanCmd += ` ${vlanName}`;
  if (allMembers.length > 0) vlanCmd += ` ${allMembers.join(' ')}`;
  
  fetchAPI("POST", "/cmd", () => {
      let untagCmd = untagged.length > 0 ? `untag ${vid} ${untagged.join(' ')}` : `untag ${vid} none`;
      fetchAPI("POST", "/cmd", () => notify(`VLAN ${vid} saved completely.`, "success"), untagCmd);
  }, vlanCmd);
  
  for (let p = 1; p <= globalNumPorts; p++) {
    if (document.getElementById(`pport${p}`).checked) {
      fetchAPI("POST", "/cmd", () => {}, `pvid ${p} ${vid}`);
    }
  }
}

function applyIGMP() {
  const isEnabled = document.getElementById('igmp-en').checked;
  fetchAPI("POST", "/cmd", () => notify(`IGMP Snooping turned ${isEnabled ? 'ON' : 'OFF'}.`, "success"), isEnabled ? "igmp on" : "igmp off");
}

function applyLAG(l) {
  let cmd = `lag ${l}`;
  for(let i=1; i<=globalNumPorts; i++) {
    if (document.getElementById(`lag${l}_p${i}`).checked) cmd += ` ${i}`;
  }
  fetchAPI("POST", "/cmd", () => notify(`LAG Group ${l} updated successfully.`, "success"), cmd);
}

function applyMirror() {
  const isEnabled = document.getElementById('mirror-en').checked;
  const destPort = document.getElementById('mirror-port').value;
  
  if (isEnabled && (!destPort || destPort < 1 || destPort > globalNumPorts)) {
     return notify("Please enter a valid destination port number.", "warning");
  }
  
  fetchAPI("POST", "/cmd", () => {}, isEnabled ? `mirror on` : `mirror off`);
  
  if (isEnabled) {
     fetchAPI("POST", "/cmd", () => {}, `mirror to ${destPort}`);
     
     let txPorts = []; let rxPorts = [];
     for(let p=1; p<=globalNumPorts; p++) {
        if (document.getElementById(`mtx${p}`).checked) txPorts.push(p);
        if (document.getElementById(`mrx${p}`).checked) rxPorts.push(p);
      }
     
     if (txPorts.length > 0) fetchAPI("POST", "/cmd", () => {}, `mirror tx ${txPorts.join(' ')}`);
     else fetchAPI("POST", "/cmd", () => {}, `mirror tx none`);
     
     if (rxPorts.length > 0) fetchAPI("POST", "/cmd", () => {}, `mirror rx ${rxPorts.join(' ')}`);
     else fetchAPI("POST", "/cmd", () => {}, `mirror rx none`);
  }
  notify("Port Mirroring settings applied.", "success");
}

function applyIP() {
  const ip = document.getElementById('sys-ip').value;
  const nm = document.getElementById('sys-mask').value;
  const gw = document.getElementById('sys-gw').value;
  const ipv4 = /^(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)){3}$/;
  if (!ipv4.test(ip) || !ipv4.test(nm) || !ipv4.test(gw)) return notify("Invalid IPv4 address format.", "error");

  fetchAPI("POST", "/cmd", () => notify("IP applied temporarily.", "success"), `ip ${ip}`);
  fetchAPI("POST", "/cmd", () => {}, `netmask ${nm}`);
  fetchAPI("POST", "/cmd", () => {}, `gw ${gw}`);
}

function rebootSwitch() {
  if (!confirm('Are you sure you want to reboot the switch? Unsaved changes will be lost.')) return;
  fetchAPI("GET", "/reset", () => notify("Switch is rebooting...", "info"));
}

/** * STRICT KEY PARSER
 * This perfectly isolates configurations. Saving "port 1 1g" will NEVER overwrite "port 1 name".
 * Mirror commands are now isolated independently.
 */
function getKey(line) {
   if (line.match(/^ip\s+/)) return "ip";
   if (line.match(/^gw\s+/)) return "gw";
   if (line.match(/^netmask\s+/)) return "netmask";
   if (line.match(/^eee\s+\d+/)) return line.match(/^eee\s+\d+/)[0];
   if (line.match(/^eee\s+(on|off)/)) return "eee";
   if (line.match(/^mirror\s+(on|off)/)) return "mirror_state";
   if (line.match(/^mirror\s+to/)) return "mirror to";
   if (line.match(/^mirror\s+tx/)) return "mirror tx";
   if (line.match(/^mirror\s+rx/)) return "mirror rx";
   if (line.match(/^vlan\s+\d+/)) return line.match(/^vlan\s+\d+/)[0];
   if (line.match(/^untag\s+\d+/)) return line.match(/^untag\s+\d+/)[0];
   if (line.match(/^pvid\s+\d+/)) return line.match(/^pvid\s+\d+/)[0];
   if (line.match(/^lag\s+\d+/)) return line.match(/^lag\s+\d+/)[0];
   if (line.match(/^bw\s+(in|out)\s+\d+/)) return line.match(/^bw\s+(in|out)\s+\d+/)[0];
   if (line.match(/^igmp\s+/)) return "igmp";
   if (line.match(/^port\s+\d+\s+name/)) return line.match(/^port\s+\d+\s+name/)[0];
   if (line.match(/^port\s+\d+/)) return line.match(/^port\s+\d+/)[0];
   if (line.match(/^mtu\s+\d+/)) return line.match(/^mtu\s+\d+/)[0];
   return null; 
}

function parseConf(s) {
  var cleanString = s.replace(/[^\x09\x0A\x0D\x20-\x7E]/g, ''); 
  var a = cleanString.split(/\r\n|\n/);
  
  for (var l = 0; l < a.length; l++) {
    var line = a[l].trim().replace(/\s+/g, ' ');
    if (!line.length) continue;
    
    let key = getKey(line);
    if (key) {
        // Strip out per-port EEE if global EEE is found
        if (key === "eee") {
            configuration = configuration.filter(item => {
               let k = getKey(item);
               return !(k && k.startsWith("eee ")); 
            });
        }
        // Deduplicate the exact key category
        configuration = configuration.filter(item => getKey(item) !== key);
    } else {
        // If it's a completely unknown command, just deduplicate exact string matches
        configuration = configuration.filter(item => item !== line);
    }
    configuration.push(line);
  }
}

async function sendConfig(c) {
  const form = new FormData();
  form.append("MAX_FILE_SIZE", "4096");
  form.append("configuration", new Blob([c], {type: "application/octet-stream"}));
  try {
    const response = await fetch('/config', { method: 'POST', body: form });
    notify("Configuration saved to flash successfully!", "success");
  } catch(err) {
    notify("Configuration saved! (Connection reset expected)", "success");
  }
}

async function flashSaveConfig() {
  const btn = document.getElementById('saveGlobalBtn');
  btn.disabled = true; notify("Merging memory and saving to flash...", "info");
  try {
    configuration = [];
    const resConfig = await fetch('/config');
    if (resConfig.ok) parseConf(await resConfig.text());
    
    const resLog = await fetch('/cmd_log');
    if (resLog.ok) parseConf(await resLog.text());

    var body = "";
    for (const x of configuration) { body = body + x + "\n"; }
    await sendConfig(body);
    setTimeout(() => { fetch('/cmd_log_clear', { method: 'GET' }).catch(()=>{}); }, 1000);
    document.getElementById('config-window').value = body;
  } catch(err) {} finally { btn.disabled = false; }
}

function saveManualConfig() {
  const btn = document.getElementById('saveBtn');
  btn.disabled = true; notify("Saving manual configuration to flash...", "info");
  let textConfig = document.getElementById('config-window').value.replace(/[^\x09\x0A\x0D\x20-\x7E]/g, ''); 
  sendConfig(textConfig).then(() => {
    setTimeout(() => { fetch('/cmd_log_clear', { method: 'GET' }).catch(()=>{}); }, 1000);
    btn.disabled = false;
  });
}

function startFlash() {
  const file = document.getElementById('binFile').files[0];
  if (!file || !file.name.toLowerCase().endsWith('.bin')) return notify("Select a valid .bin file.", "error");
  if (!confirm("WARNING: Flashing interrupts traffic. Do not power off. Proceed?")) return;

  isFlashing = true; clearInterval(poller); clearInterval(systemInterval);
  reqQ = []; inFlight = null; 

  const bar = document.getElementById('fBar'), text = document.getElementById('fText'), status = document.getElementById('fStatus');
  document.getElementById('flashBtn').disabled = true;
  document.getElementById('progress-wrap').style.display = 'block';

  status.innerText = "UPLOADING FIRMWARE TO RAM...";

  const xhr = new XMLHttpRequest(); 
  xhr.open("POST", "/upload", true);
  let fNote = false;

  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const p = Math.round((e.loaded / e.total) * 100);
      const visualP = Math.round(p * 0.2); 
      
      bar.style.width = visualP + "%"; text.innerText = visualP + "%";
      
      if (p === 100 && !fNote) { 
        fNote = true; status.innerText = "WRITING TO FLASH. DO NOT UNPLUG!"; 
        notify("Upload complete. Flashing memory...", "warning"); 
        
        let fakeP = 20;
        const rebootTimer = setInterval(() => {
          if (++fakeP <= 99) { bar.style.width = fakeP + "%"; text.innerText = fakeP + "%"; }
          if (fakeP === 40) status.innerText = "REBOOTING SWITCH... PLEASE WAIT.";
          if (fakeP === 100) {
             clearInterval(rebootTimer);
             status.innerText = "REBOOT COMPLETE. RELOADING...";
             bar.style.background = "var(--success)";
             setTimeout(() => window.location.href = '/login.html', 2000);
          }
        }, 600); 
      }
    }
  };
  const fd = new FormData(); 
  fd.append("MAX_FILE_SIZE", "1000000"); 
  fd.append("uploadedfile", new Blob([file], { type: "application/octet-stream" }), "update.bin"); 
  xhr.send(fd);
}

const mib_counters = [ "In Octets", 8, "Out Octets", 8, "In Unicast Pkts", 8, "In Multicast Pkts", 8, "In Broadcast Pkts", 8, "Out Unicast Pkts", 8, "Out Multicast Pkts", 8, "Out Broadcast Pkts", 8, "Out discards", 4, "802.1d Tp Port discards", 4, "802.3 Single collision", 4, "802.3 Multi collision", 4, "802.3 Deferred tx", 4, "802.3 Late collisions", 4, "802.3 Excessive collisions", 4, "802.3 Symbol errors", 4, "802.3 Control in unk", 4, "802.3 In Pause frames", 4, "802.3 Out Pause frames", 4, "Ether drop events", 4, "TX Broadcast Pkts", 4, "TX Multicast Pkts", 4, "TX CRC Align errors", 4, "RX CRC Align errors", 4, "TX Undersized Pkts", 4, "RX Undersized Pkts", 4, "TX Oversized Pkts", 4, "RX Oversized Pkts", 4, "TX Fragments", 4, "RX fragments", 4, "TX Jabbers", 4, "RX Jabbers", 4, "TX Collisions", 4, "TX 640 Octets", 4, "RX 640 Octets", 4, "TX 65-127 Octets", 4, "RX 65-127 Octets", 4, "TX 128-255 Octets", 4, "RX 128-255 Octets", 4, "TX 256-511 Octets", 4, "RX 256-511 Octets", 4, "TX 512-1023 Octets", 4, "RX 512-1023 Octets", 4, "TX 1024-1518 Octets", 4, "RX 1024-1518 Octets", 4, "", 4, "RX Undersized Drops", 4, "TX >1518 Octets", 4, "RX >1518 Octets", 4, "TX Pkts too large", 4, "RX Pkts too large", 4, "TX Flex Octets S1", 4, "RX Flex Octets S1", 4, "TX Flex CRC S1", 4, "RX Flex CRC S1", 4, "TX Flex Octets S0", 4, "RX Flex Octets S0", 4, "TX Flex CRC S0", 4, "RX Flex CRC S0", 4, "Lenth Field Errors", 4, "False Carriers", 4, "Undersized Octets", 4, "Framing Errors", 4, "", 4, "RX MAC Discards", 4, "RX MAC IPG Short Drop", 4, "", 4, "802.1d TP Learned Discard", 4, "EQ 7 Dropped Pkts", 4, "EQ 6 Dropped Pkts", 4, "EQ 5 Dropped Pkts", 4, "EQ 4 Dropped Pkts", 4, "EQ 3 Dropped Pkts", 4, "EQ 2 Dropped Pkts", 4, "EQ 1 Dropped Pkts", 4, "EQ 0 Dropped Pkts", 4, "EQ 7 Out Pkts", 4, "EQ 6 Out Pkts", 4, "EQ 5 Out Pkts", 4, "EQ 4 Out Pkts", 4, "EQ 3 Out Pkts", 4, "EQ 2 Out Pkts", 4, "EQ 1 Out Pkts", 4, "EQ 0 Out Pkts", 4, "TX Good Counter", 8, "RX Good Counter", 8, "RX Error Counter", 4, "TX Error Counter", 4, "TX Good PHY", 8, "RX Good PHY", 8, "RX Error PHY", 4, "TX Error PHY", 4 ];

function showCounters(p) {
  const queryPort = physToLogMap[p] !== undefined ? physToLogMap[p] : (p - 1);
  document.getElementById('statsModalTitle').textContent = `Detailed Counters: Port ${p}`;
  document.getElementById('statsModal').style.display = "flex";
  fetchAPI("GET", `/counters.json?port=${queryPort}`, (raw) => {
    try {
      const s = JSON.parse(raw); let t = `<table class="mib-table"><tbody><tr>`; let c = 0;
      for (let i = 0; i < mib_counters.length; i += 2) {
        if (!mib_counters[i]) continue;
        let count = s[Math.floor(i / 2)] ? BigInt(s[Math.floor(i / 2)]) : 0n;
        if (mib_counters[i+1] === 8) { t += `<td>${mib_counters[i]}</td><td><strong>${count.toString()}</strong></td>`; c++; } 
        else { t += `<td>${mib_counters[i]}</td><td><strong>${(count & 4294967295n).toString()}</strong></td>`; c++; }
        if (c === 2) { t += "</tr><tr>"; c = 0; }
      }
      document.getElementById('statsModalBody').innerHTML = t + "</tr></tbody></table>";
    } catch (e) {}
  });
}
function closeCounters() { document.getElementById('statsModal').style.display = "none"; }

window.onload = () => { nav('dash'); };

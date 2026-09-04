var systemInterval = Number();
var isSaving = false;
const ips = ["ip", "netmask", "gw"];

function changeLang() {
  var lang = document.getElementById('lang-select').value;
  setLang(lang);
}

function checkIp(ip) {
  const ipv4 = /^(\d{1,3}\.){3}\d{1,3}$/;
  if (!ipv4.test(ip)) {alert(t('sys_invalid_ip') + ip); return false };
  return true;
}

async function ipSub() {
  for (let i=0;i<3;i++) {
    if (!checkIp(document.getElementById(ips[i]).value))
      return;
  }
  var cmd = '';
  for (let i=0; i<3;i++){
    cmd += ips[i]+' '+document.getElementById(ips[i]).value+'\n';
  }
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
    fetchIP();
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

async function cmdSub() {
  const input = document.getElementById('console_cmd');
  const out = document.getElementById('console_out');
  const cmd = input.value;
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    if (response.status == 401) {
      window.location.href = 'login.html';
      return;
    }
    let text = await response.text();
    if (text != "" && !text.endsWith("\n"))
      text += "\n";
    if (out.textContent.length > 20000)
      out.textContent = out.textContent.slice(-16000);
    out.textContent += "> " + cmd + "\n" + text;
    out.scrollTop = out.scrollHeight;
    input.value = "";
  } catch(err) {
      out.textContent += "> " + cmd + "\n" + err + "\n";
      console.error(`Error: ${err}`);
  }
}


async function hostSub() {
  const h = document.getElementById("hostname").value;
  try { await fetch('/cmd', { method: 'POST', body: "hostname " + h }); }
  catch(err) { console.error(`Error: ${err}`); }
  fetchIP();
}


async function sendConfig(c) {
  if (isSaving) return;
  isSaving = true;
  clearInterval(systemInterval);
  const form = new FormData();
  form.append("MAX_FILE_SIZE", "4096");
  form.append("configuration", new Blob([c], {type: "application/octet-stream"}), "config.txt");
  try {
    const response = await fetch('/config', {
      method: 'POST',
      body: form
    });
    console.log('Completed!', response);
    try {
      await fetch('/cmd_log_clear', { method: 'GET' });
    } catch(e) {}
  } catch(err) {
    console.error(`Error: ${err}`);
  } finally {
    isSaving = false;
    systemInterval = setInterval(fetchIP, 1000);
  }
}


async function flashSave() {
  configuration = [];
  const savedConfig = await fetchConfig();
  const cmdLog = await fetchCmdLog();
  if (savedConfig) parseConf(savedConfig);
  if (cmdLog) parseConf(cmdLog);
  const body = configuration.join('\n') + '\n';
  console.log("CONFIGURATION to save: ", body);
  await sendConfig(body);
}

async function flashStartupSave() {
  var configContent = document.getElementById("config_display").value;
  console.log("CONFIGURATION to save: ", configContent);
  sendConfig(configContent);
  // Clear the command log 1 second after initiating the config save
  setTimeout(() => {
    fetch('/cmd_log_clear', { method: 'GET' })
      .then(response => console.log('Command log cleared', response))
      .catch(err => console.error('Error clearing command log:', err));
  }, 1000);
}

function clearConfig() {
  document.getElementById("config_display").value = "";
  
  // Validate and populate with current IP settings
  for (let i=0; i<3; i++) {
    if (!checkIp(document.getElementById(ips[i]).value))
      return;
  }
  
  var configLines = "";
  for (let i=0; i<3; i++){
    var cmd = ips[i]+' '+document.getElementById(ips[i]).value;
    configLines += cmd + "\n";
  }
  
  document.getElementById("config_display").value = configLines;
}

function fetchIP() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("IP: ", s);
      document.getElementById("ip").value=s.ip_address;
      document.getElementById("netmask").value=s.ip_netmask;
      document.getElementById("gw").value=s.ip_gateway;
      document.getElementById("hostname").value=s.hostname;
      document.getElementById("model").textContent=s.hw_ver;
      loadMgmtVlan();
      clearInterval(systemInterval);
      // Fetch and populate the config textbox
      fetchConfig().then((configText) => {
        let fullConfig = configText;
        // Fetch and append cmd_log
        //return fetchCmdLog().then((cmdLogText) => {
        //  if (cmdLogText) {
        //    fullConfig = fullConfig + cmdLogText;
        //  }
        document.getElementById("config_display").value = fullConfig;
        });
      };
    }
  xhttp.open("GET", `/information.json`, true);
  xhttp.send();
}

function resetSwitch() {
  if (!confirm(t('sys_reset_confirm'))) {
    return;
  }
  fetch('/reset', { method: 'GET' }).catch(() => {});
  setTimeout(() => {
    alert(t('sys_resetting'));
  }, 3000);
}

window.addEventListener("load", function() {
  var langSel = document.getElementById('lang-select');
  if (langSel) langSel.value = rtlLang;
  systemInterval = setInterval(fetchIP, 1000);
});


var mgmtVlanCurrent = 0;

function loadMgmtVlan() {
  var sel = document.getElementById('mgmtvlan');
  if (!sel) return;
  fetch('/vlanlist').then(function(r) { return r.json(); }).then(function(d) {
    var cur = d.mgmt || 0;
    var list = d.vlan || [];
    mgmtVlanCurrent = cur;
    sel.innerHTML = '';
    if (!cur) {
      var none = document.createElement('option');
      none.value = 0; none.disabled = true;
      none.textContent = t('sys_mgmt_untagged');
      sel.appendChild(none);
    }
    for (var i = 0; i < list.length; i++) {
      var o = document.createElement('option');
      o.value = list[i].id;
      o.textContent = list[i].name ? (list[i].id + ' (' + list[i].name + ')') : list[i].id;
      sel.appendChild(o);
    }
    sel.value = cur;
  }).catch(function(err) { console.error('VLAN list failed:', err); });
}

function mgmtVlanChanged() {
  var sel = document.getElementById('mgmtvlan');
  var id = parseInt(sel.value, 10);
  if (!id || id === mgmtVlanCurrent) return;
  if (!confirm(t('sys_mgmt_confirm') + id + '.\n\n' + t('sys_mgmt_warn'))) {
    sel.value = mgmtVlanCurrent;
    return;
  }
  fetch('/cmd', { method: 'POST', body: 'vlan ' + id + ' mgmt' })
    .then(function() { mgmtVlanCurrent = id; })
    .catch(function(err) { console.error('Set management VLAN failed:', err); sel.value = mgmtVlanCurrent; });
}

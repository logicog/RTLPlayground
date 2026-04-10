var systemInterval = Number();
const ips = ["ip", "netmask", "gw"];

function checkIp(ip) {
  const ipv4 = /^(\d{1,3}\.){3}\d{1,3}$/;
  if (!ipv4.test(ip)) {alert(`Invalid ip:${ip}`); return false };
  return true;
}

async function ipSub() {
  for (let i=0;i<3;i++) {
    if (!checkIp(document.getElementById(ips[i]).value))
      return;
  }
  for (let i=0; i<3;i++){
    var cmd = ips[i]+' '+document.getElementById(ips[i]).value;
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
}

async function sendConfig(c) {
    const form = new FormData();
  form.append("MAX_FILE_SIZE", "4096");
  form.append("configuration", new Blob([c], {type: "application/octet-stream"}));
  try {
    const response = await fetch('/config', {
      method: 'POST',
      body: form
    });
    console.log('Completed!', response);
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}


async function flashSave() {
  fetchConfig().then((s) => {
    parseConf(s);
    fetchCmdLog().then((s) => {
      parseConf(s);
      var body = "";
      for (const x of configuration) { body = body + x + "\n"; }
      console.log("CONFIGURATION to save: ", body);
      sendConfig(body);
    });
  });
  setTimeout(() => {
      fetchIP();
  }, 500);
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
  if (!confirm('Are you sure you want to reset the switch?')) {
    return;
  }
  fetch('/reset', { method: 'GET' }).catch(() => {});
  setTimeout(() => {
    alert('Switch is resetting. Please wait and refresh the page.');
  }, 3000);
}

async function sendCmd(cmd) {
  try {
    const response = await fetch('/cmd', { method: 'POST', body: cmd });
    console.log('cmd sent:', cmd, response);
  } catch (err) {
    console.error(`Error sending cmd '${cmd}':`, err);
  }
}

function svcToggleStp(enabled) {
  sendCmd(enabled ? 'stp on' : 'stp off').then(fetchServices);
}

function svcToggleLbd(enabled) {
  if (enabled) {
    const timer = document.getElementById('svc_lbd_timer').value || 1000;
    sendCmd('lbd on ' + timer).then(fetchServices);
  } else {
    sendCmd('lbd off').then(fetchServices);
  }
}

function svcApplyMgmtVlan() {
  const vlan = parseInt(document.getElementById('svc_mgmt_vlan').value || '0', 10);
  if (isNaN(vlan) || vlan < 0 || vlan > 4094) {
    alert('Invalid VLAN ID');
    return;
  }
  sendCmd('vlan ' + vlan + ' mgmt').then(fetchServices);
}

function svcToggleDhcpd(enabled) {
  if (enabled) {
    const start = parseInt(document.getElementById('svc_dhcpd_pool_start').value || '100', 10);
    const count = parseInt(document.getElementById('svc_dhcpd_pool_count').value || '8', 10);
    const lease = parseInt(document.getElementById('svc_dhcpd_lease').value || '3600', 10);
    sendCmd('dhcpd on ' + start + ' ' + count + ' ' + lease).then(fetchServices);
  } else {
    sendCmd('dhcpd off').then(fetchServices);
  }
}

function fetchServices() {
  fetch('/services.json')
    .then(r => r.json())
    .then(s => {
      document.getElementById('svc_stp').checked = !!s.stp;
      document.getElementById('svc_lbd').checked = !!s.lbd;
      if (s.lbd_timer) document.getElementById('svc_lbd_timer').value = s.lbd_timer;
      document.getElementById('svc_mgmt_vlan').value = s.mgmt_vlan || 0;
      document.getElementById('svc_dhcpd').checked = !!s.dhcpd;
      if (s.dhcpd_pool_start) document.getElementById('svc_dhcpd_pool_start').value = s.dhcpd_pool_start;
      if (s.dhcpd_pool_count) document.getElementById('svc_dhcpd_pool_count').value = s.dhcpd_pool_count;
      if (s.dhcpd_lease_time) document.getElementById('svc_dhcpd_lease').value = s.dhcpd_lease_time;
      document.getElementById('svc_dhcpd_leases').textContent = s.dhcpd_leases || 0;
    })
    .catch(err => console.error('Error fetching services:', err));
}

window.addEventListener("load", function() {
  systemInterval = setInterval(fetchIP, 1000);
  fetchServices();
  setInterval(fetchServices, 5000);
});

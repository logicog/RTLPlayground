var systemInterval = Number();
const ips = ["ip", "netmask", "gw"];

function checkIp(ip) {
  const ipv4 = /^(\d{1,3}\.){3}\d{1,3}$/;
  if (!ipv4.test(ip)) {alert(`Invalid ip:${ip}`); return false };
  return true;
}

async function ipSub() {
  if (document.getElementById('dhcp').checked) {
    var cmd = "ip dhcp";
    try {
      const response = await fetch('/cmd', {
        method: 'POST',
        body: cmd
      });
      console.log('Completed!', response);
      systemInterval = setInterval(fetchIP, 10000);
    } catch(err) {
      console.error(`Error: ${err}`);
    }
    return;
  }
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
async function dhcpdSub() {
  var dhcpd_cmd = "dhcpd off";
  if (document.getElementById('dhcpd').checked) {
    dhcpd_cmd = "dhcpd on";
    var v=document.getElementById('dhcpd_vid').value
    if (v && v!= 0)
      dhcpd_cmd = dhcpd_cmd + " " + v;
  }
  try {
    console.log("Sending: ", dhcpd_cmd);
    const response = await fetch('/cmd', {
      method: 'POST',
      body: dhcpd_cmd
    });
    console.log('Completed!', response);
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}
function dhcpClicked(e)
{
  console.log("dhcpClicked called");
  if (e.checked) {
    for (let i=0; i<3;i++)
      document.getElementById(ips[i]).disabled = true;
    document.getElementById('dhcpd').disabled = true;
  } else {
    console.log("dhcpClicked off");
    for (let i=0; i<3;i++)
      document.getElementById(ips[i]).disabled = false;
    document.getElementById('dhcpd').disabled = false;
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
      document.getElementById('dhcp').checked = s.dhcp_client;
      document.getElementById('dhcpd').checked = s.dhcp_server;
      clearInterval(systemInterval);
    }
  }
  xhttp.open("GET", `/information.json`, true);
  xhttp.send();
}

window.addEventListener("load", function() {
  systemInterval = setInterval(fetchIP, 1000);
});

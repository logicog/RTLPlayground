var systemInterval = Number();
const ips = ["ip", "nm", "gw"];

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

async function flashSave() {
  fetchConfig();
  fetchCmdLog();
  return;
  try {
    const response = await fetch('/save', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

function fetchIP() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("IP: ", s);
      document.getElementById("ip").value=s.ip_address;
      document.getElementById("nm").value=s.ip_netmask;
      document.getElementById("gw").value=s.ip_gateway;
      clearInterval(systemInterval);
    }
  }
  xhttp.open("GET", `/information.json`, true);
  xhttp.send();
}

window.addEventListener("load", function() {
  systemInterval = setInterval(fetchIP, 1000);
});

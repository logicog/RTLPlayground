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
    } catch(err) {
      console.error(`Error: ${err}`);
    }
  }
}

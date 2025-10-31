var systemInterval = Number();
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

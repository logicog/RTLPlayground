var txG = new BigInt64Array(10);
var txB = new BigInt64Array(10);
var rxG = new BigInt64Array(10);
var rxB = new BigInt64Array(10);
const linkS = ["Disabled", "No Link", "100M", "1000M", "500M", "10G", "2.5G", "5G"];
var pState = new Int8Array(10);
var pIsSFP = new Int8Array(10);
var numPorts = 0;
const logToPhysPort = [0, 0, 0, 5, 1, 2, 3, 4, 6];
const physToLogPort = [	4, 5, 6, 7, 3, 8];
function drawPorts() {
  var f = document.getElementById('ports');
  console.log("DRAWING PORTS: ", numPorts);
  for (let i = 0; i < numPorts; i++) {
    console.log("DRAWING isSFP: ", pIsSFP[i]);
    const d = document.createElement("div");
    d.classList.add('tooltip');
    const s = document.createElement("span");
    s.classList.add("tooltiptext");
    s.innerHTML = "Tooltip text";
    s.id="tt_" + (i+1);
    const l = document.createElement("object");
    d.appendChild(l);
    d.appendChild(s);
    l.type = "image/svg+xml";
    if (!pIsSFP[i]) {
      l.data = "port.svg";
      l.width ="40";
      l.height ="40";
    } else {
      l.data = "sfp.svg";
      l.width = "60";
      l.height = "60";
    }
    l.id="port" + (i+1);
    f.appendChild(d);
  }
}

function update() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 401)
	    document.location = "/login.html"
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      if (!numPorts) {
	numPorts = s.length;
	for (let i = 0; i < s.length; i++)
	  pIsSFP[s[i].portNum-1] = s[i].isSFP;
	drawPorts();
      }
      console.log("RES:", JSON.stringify(s));
      for (let i = 0; i < s.length; i++) {
	p = s[i];
	let n = p.portNum;
	let pid = "port" + n;
	let ttid = "tt_" + n;
	n--;
	txG[n] = BigInt(p.txG); txB[n] = BigInt(p.txB); rxG[n] = BigInt(p.rxG); rxB[n] = BigInt(p.rxB);
	var psvg = document.getElementById(pid);
	var tt = document.getElementById(ttid);
	if (psvg == null || !psvg.contentDocument)
	  continue;
	var bgs = psvg.contentDocument.getElementsByClassName("bg");
	var leds = psvg.contentDocument.getElementsByClassName("led");
	if (p.enabled == 0) {
	  pState[n] = -1;
	  bgs[0].style.fill = "red";
	  leds[0].style.fill = "black"; leds[1].style.fill = "black";
	  psvg.style.opacity = 0.4;
	  tt.innerHTML = "Not enabled.";
	} else {
	  psvg.style.opacity = 1.0;
	  pState[n] = p.link;
	  if (p.link == 4 || p.link == 5 || p.link == 6) {
	    leds[0].style.fill = "green"; leds[1].style.fill = "orange";
	  } else if (p.link == 1 || p.link == 2 || p.link == 3) {
	    leds[0].style.fill = "green"; leds[1].style.fill = "green";
	  } else {
	    leds[0].style.fill = "black"; leds[1].style.fill = "black";
	    psvg.style.opacity = 0.4
	  }
	  var iHTML = "<table border=\"0\" class=\"tt_table\">";
	  iHTML += "<tr><td align=\"left\">Link speed</td><td>:</td><td>" + linkS[p.link + 1] + "</td></tr>";
	  if (p.isSFP) {
	    iHTML += "<tr><td>Vendor</td><td>:</td><td>" + p.sfp_vendor + "</td></tr>";
	    iHTML += "<tr><td>Model</td><td>:</td><td>" + p.sfp_model + "</td></tr>";
	    iHTML += "<tr><td>Serial</td><td>:</td><td>" + p.sfp_serial + "</td></tr>";
	    if (p.sfp_options & 0x40) {
	      iHTML += "<tr><td>Temp</td><td>:</td><td>" + (Number(p.sfp_temp) >> 8) + "." + ((Number(p.sfp_temp) & 0xff)/256.0 * 100).toFixed(0) + "&#8239;&#8451;</td></tr>";
	      iHTML += "<tr><td>Vcc</td><td>:</td><td>" + (Number(p.sfp_vcc) / 10000.0).toFixed(2) + "&#8239;V</td></tr>";
	      iHTML += "<tr><td>TX-Bias</td><td>:</td><td>" + (Number(p.sfp_txbias) / 500.0).toFixed(1) + "&#8239;mA</td></tr>";
	      iHTML += "<tr><td>TX-Power</td><td>:</td><td>" + (Number(p.sfp_txpower) / 10.0).toFixed(0) + "&#8239;mW</td></tr>";
	      iHTML += "<tr><td>RX-Power</td><td>:</td><td>" + (Number(p.sfp_rxpower) / 10.0).toFixed(0) + "&#8239;mW</td></tr>";
	    }
	  }
	  iHTML += "</table>";
	  tt.innerHTML = iHTML;
	}
      }
    }
  };
  xhttp.open("GET", "/status.json", true);
  xhttp.timeout = 5000; xhttp.send();
}

window.addEventListener("load", function() {
  update();
  const interval = setInterval(update, 2000);
});

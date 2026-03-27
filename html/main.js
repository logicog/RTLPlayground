var txG = new BigInt64Array(10);
var txB = new BigInt64Array(10);
var rxG = new BigInt64Array(10);
var rxB = new BigInt64Array(10);
const linkS = ["Disabled", "Down", "10M", "100M", "1000M", "500M", "10G", "2.5G", "5G"];
var pState = new Int8Array(10);
var pIsSFP = new Int8Array(10);
var pAdvertised = new Int8Array(10);
var numPorts = 0;
var logToPhysPort = new Int8Array(10);
var physToLogPort = new Int8Array(10);
var currentRequests = [];
var currentCallback;
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

function parseUint16(val) {
  return parseInt(val, 16) & 0xffff;
}

function parseInt16(val) {
  let valInt = parseInt(val, 16);
  let num = valInt & 0x7fff;
  if (valInt & 0x8000) {
    return num - 0x8000;
  }
  return num;
}

function applyCalibrationSlopeOffset(val, cal) {
  if (typeof cal !== 'string') {
    return val;
  }
  if (cal.startsWith("0x")) {
    cal = cal.substring(2);
  }
  if (cal.length != 8) {
    return val;
  }
  let slope = parseUint16(cal.substring(0, 4)) / 256;
  let offset = parseInt16(cal.substring(4, 8));
  return slope * val + offset;
}

function applyRxPowerCalibration(val, cal) {
  if (typeof cal !== 'string') {
    return val;
  }
  if (cal.startsWith("0x")) {
    cal = cal.substring(2);
  }
  if (cal.length != 40) {
    return val;
  }
  let bytes = cal.match(/.{1,2}/g).map(function (x) { return parseInt(x, 16); });
  let view = new DataView(new Uint8Array(bytes).buffer);
  return view.getFloat32(0) * Math.pow(val, 4)
    + view.getFloat32(4) * Math.pow(val, 3)
    + view.getFloat32(8) * Math.pow(val, 2)
    + view.getFloat32(12) * val
    + view.getFloat32(16);
}

function decodeSfpTemp(val, cal) {
  let temp = parseInt16(val);
  return applyCalibrationSlopeOffset(temp, cal) / 256;
}

function decodeSfpVcc(val, cal) {
  let vcc = parseUint16(val);
  return applyCalibrationSlopeOffset(vcc, cal) / 10000;
}

function decodeSfpTxBias(val, cal) {
  let bias = parseUint16(val);
  return applyCalibrationSlopeOffset(bias, cal) / 500;
}

function decodeSfpTxPower(val, cal) {
  let txPower = parseUint16(val);
  return applyCalibrationSlopeOffset(txPower, cal) / 10000;
}

function decodeSfpRxPower(val, cal) {
  let rxPower = parseUint16(val);
  return applyRxPowerCalibration(rxPower, cal) / 10000;
}

function update(callback) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    console.log("IN UPDATE ");
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
	logToPhysPort[p.logPort] = n;
	physToLogPort[n-1] = p.logPort;
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
        if (leds[0] == null || leds[0].style == null)
          continue;
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
	    pAdvertised[n] = 0;
	    const hasExtendedStatus = p.sfp_options & 0x40;
	    iHTML += "<tr><td>Vendor</td><td>:</td><td>" + p.sfp_vendor + "</td></tr>";
	    iHTML += "<tr><td>Model</td><td>:</td><td>" + p.sfp_model + "</td></tr>";
	    iHTML += "<tr><td>Serial</td><td>:</td><td>" + p.sfp_serial + "</td></tr>";
	    if (hasExtendedStatus) {
	      iHTML += "<tr><td>Temp</td><td>:</td><td>" + decodeSfpTemp(p.sfp_temp, p.sfp_temp_cal).toFixed(2) + "&#8239;&#8451;</td></tr>";
	      iHTML += "<tr><td>Vcc</td><td>:</td><td>" + decodeSfpVcc(p.sfp_vcc, p.sfp_vcc_cal).toFixed(2) + "&#8239;V</td></tr>";
	      iHTML += "<tr><td>TX-Fault</td><td>:</td><td>" + (Boolean(Number(p.sfp_state) & 0x4)) + "</td></tr>";
	      iHTML += "<tr><td>TX-Disabled</td><td>:</td><td>" + (Boolean(Number(p.sfp_state) & 0x80)) + "</td></tr>";
	      iHTML += "<tr><td>TX-Bias</td><td>:</td><td>" + decodeSfpTxBias(p.sfp_txbias, p.sfp_txbias_cal).toFixed(1) + "&#8239;mA</td></tr>";
	      iHTML += "<tr><td>TX-Power</td><td>:</td><td>" + decodeSfpTxPower(p.sfp_txpower, p.sfp_txpower_cal).toFixed(3) + "&#8239;mW</td></tr>";
	      iHTML += "<tr><td>RX-Power</td><td>:</td><td>" + decodeSfpRxPower(p.sfp_rxpower, p.sfp_rxpower_cal).toFixed(3) + "&#8239;mW</td></tr>";
	    }
	    // Not all devices & modules have LOS pin...
	    const rx_los_pin = p.sfp_los !== null ? Boolean(Number(p.sfp_los)) : null;
	    const rx_los_module = hasExtendedStatus ? Boolean(Number(p.sfp_state) & 0x2) : null;
	    if (rx_los_module !== null || rx_los_pin !== null) {
	      iHTML += `<tr><td>RX-LOS</td><td>:</td><td>${rxLosHTML(rx_los_pin, rx_los_module)}</td></tr>`;
	    }
	  } else {
	    pAdvertised[n] = parseInt(p.adv, 2);
	  };
	  iHTML += "</table>";
	  tt.innerHTML = iHTML;
	}}
	if (callback)
	  callback();
	}};
	xhttp.open("GET", "/status.json", true);
	xhttp.timeout = 5000;
	sendXHTTP(xhttp);
}

function rxLosHTML(pinStatus, moduleStatus) {
  if (moduleStatus !== null && pinStatus !== null && moduleStatus !== pinStatus) {
    return `pin=${pinStatus}<br/>mod=${moduleStatus}<br/>❗❗❗❗`;
  }

	// Returns first non null value
  return moduleStatus ?? pinStatus;
}

function callbackXHTTP()
{
  x = currentRequests.shift();
  x.onreadystatechange = currentCallback;
  x.onreadystatechange();
  if (currentRequests.length === 0)
    return;
  x = currentRequests[0];
  currentCallback = x.onreadystatechange;
  x.onreadystatechange = callbackXHTTP;
  var retries = 10;
  while (retries) {
    try {
      setTimeout(() => {
              x.send();
              console.log("B1");
      }, 20);
    } catch (error) {
      retries--;
      setTimeout(() => {
        console.log(`Retry ${retries}/${maxRetries} failed: ${error.message}`);
      }, 200);
      if (retries < 1) {
        throw error;
      }
    }
    console.log("B2");
    return;
  }
}

function sendXHTTP(x)
{
  console.log("sendXHTTP ", x);
  if (currentRequests.length === 0) {
    currentRequests.push(x);
    currentCallback = x.onreadystatechange;
    x.onreadystatechange = callbackXHTTP;
    var retries = 10;
    while (retries) {
      try {
        x.send();
        console.log("A1");
      } catch (error) {
        retries--;
        setTimeout(() => {
          console.log(`Retry ${retries}/${maxRetries} failed: ${error.message}`);
        }, 200);
        if (retries < 1) {
          throw error;
        }
      }
      console.log("A2");
      return;
    }
    console.log("A3");
    return;
  }
  currentRequests.push(x);
}


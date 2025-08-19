var txG = new BigInt64Array(10);
var txB = new BigInt64Array(10);
var rxG = new BigInt64Array(10);
var rxB = new BigInt64Array(10);
const linkS = ["Disabled", "No Link", "100M", "1000M", "NO", "NO", "2.5G"];
var pState = new Int8Array(10);
var pIsSFP = new Int8Array(10);
var numPorts = 0;

function update() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      if (!numPorts) {
	numPorts = s.length;
	for (let i = 0; i < s.length; i++)
	  pIsSFP[i] = s[i].isSFP;
	drawPorts();
      }
      console.log("Request returned", JSON.stringify(s));
      for (let i = 0; i < s.length; i++) {
	p = s[i];
	let pid = "port" + p.portNum;
	txG[i] = BigInt(p.txG); txB[i] = BigInt(p.txB); rxG[i] = BigInt(p.rxG); rxB[i] = BigInt(p.rxB);
	var psvg = document.getElementById(pid);
	if (psvg == null || !psvg.contentDocument)
	  continue;
	var bgs = psvg.contentDocument.getElementsByClassName("bg");
	var leds = psvg.contentDocument.getElementsByClassName("led");
	if (p.enabled == 0) {
	  pState[i] = -1;
	  bgs[0].style.fill = "red";
	  leds[0].style.fill = "black";
	  leds[1].style.fill = "black";
	  psvg.style.opacity = 0.4;
	} else {
	  psvg.style.opacity = 1.0;
	  pState[i] = p.link;
	  if (p.link == 5) {
	    leds[0].style.fill = "green"; leds[1].style.fill = "orange";
	  } else if (p.link == 2) {
	    leds[0].style.fill = "green"; leds[1].style.fill = "green";
	  } else {
	    leds[0].style.fill = "black"; leds[1].style.fill = "black";
	    psvg.style.opacity = 0.4
	  }
	  console.log(JSON.stringify(leds));
	}
      }
    }
  };
  xhttp.open("GET", "/status.json", true);
  xhttp.send(); 
}

window.addEventListener("load", function() {
  update();
  const interval = setInterval(update, 1000);
});

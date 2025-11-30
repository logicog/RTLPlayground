const logToPhysPort = [0, 0, 0, 5, 1, 2, 3, 4, 6];
const physToLogPort = [	4, 5, 6, 7, 3, 8];

function drawPorts() {
  var f = document.getElementById('ports');
  console.log("DRAWING PORTS: ", numPorts);
  for (let i = 0; i < numPorts; i++) {
    console.log("DRAWING isSFP: ", pIsSFP[i]);
    const l = document.createElement("object");
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
    f.appendChild(l);
  }
  console.log("DRAWING DONE ");
}

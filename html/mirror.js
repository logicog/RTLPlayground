var mirrorInterval = Number();
const mirrors = ["mPortsTX", "mPortsRX"];

function mirrorForm() {
  if (!numPorts)
    return;
  clearInterval(mirrorInterval);
  for (let j=0; j < mirrors.length; j++) {
    console.log("Adding Mirror " + j)
    var m = document.getElementById(mirrors[j]);
      for (let i = 1; i <= numPorts; i++) {
      const d = document.createElement("div");
      d.classList.add("cbgroup");
      const l = document.createElement("label");
      l.innerHTML = "" + i;
      l.classList.add("cbgroup");
      const inp = document.createElement("input");
      inp.type = "checkbox"; inp.setAttribute("class","psel");
      inp.id = mirrors[j] + i;
      const o = document.createElement("img");
      if (pIsSFP[i - 1]) {
        o.src = "sfp.svg"; o.width ="60"; o.height ="60";
      } else {
        o.src = "port.svg"; o.width = "40"; o.height = "40";
      }
      l.appendChild(inp); l.appendChild(o);
      d.appendChild(l)
      m.appendChild(d);
    }
  }
  fetchMirror();
}

function setM(p, c){
  document.getElementById(p).checked=c;
}
window.addEventListener("load", function() {
  mirrorInterval = setInterval(mirrorForm, 200);
});

function fetchMirror() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("MIRROR: ", JSON.stringify(s));
      document.getElementById('me').checked = s.enabled;
      document.getElementById('mp').value = s.mPort;
      let m_tx = parseInt(s.mirror_tx, 2);
      let m_rx = parseInt(s.mirror_rx, 2);
      for (let i = 1; i <= numPorts; i++) {
        let p = i - 1;
        if (numPorts < 9)
          p = physToLogPort[p];
        setM("mPortsTX"+i, m_tx&(1<<p)); setM("mPortsRX"+i, m_rx&(1<<p));
      }
    }
  };
  xhttp.open("GET", `/mirror.json`, true);
  xhttp.send();
}

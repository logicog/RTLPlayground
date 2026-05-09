var vlanInterval = Number();

function vlanForm() {
  if (!numPorts)
    return;
  var t = document.getElementById('tPorts');
  var u = document.getElementById('uPorts');
  var p = document.getElementById('pPorts');
  for (let i = 1; i <= numPorts; i++) {
    const d = document.createElement("div");
    d.classList.add("cbgroup");
    const l = document.createElement("label");
    l.innerHTML = "" + i;
    l.classList.add("cbgroup");
    const inp = document.createElement("input");
    inp.type = "checkbox"; inp.setAttribute("class","psel");
    inp.id = "tport" + i;
    inp.setAttribute('onclick', `setC("u", ${i}, false);`);
    const o = document.createElement("img");
    if (pIsSFP[i - 1]) {
      o.src = "sfp.svg"; o.width ="60"; o.height ="60";
    } else {
      o.src = "port.svg"; o.width = "40"; o.height = "40";
    }
    l.appendChild(inp); l.appendChild(o);
    d.appendChild(l)
    t.appendChild(d);
    var d2=d.cloneNode(true);
    d2.children[0].children[0].id = "uport" + i;
    d2.children[0].children[0].setAttribute('onclick', `setC("t", ${i}, false);`);
    u.appendChild(d2);
    var d3=d.cloneNode(true);
    d3.children[0].children[0].id = "pport" + i;
    d3.children[0].children[0].removeAttribute('onclick');
    p.appendChild(d3);
  }
}

function setC(t, p, c){
  document.getElementById(t+'port'+p).checked=c;
}

function utClicked(t){
  for (let i = 1; i <= numPorts; i++) {
    setC('t', i, t); setC('u', i, !t);
  }
}

function pvClicked(p){
  for (let i = 1; i <= numPorts; i++) {
    setC('p', i, p);
  }
}

function fetchVLAN() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("VLAN: ", JSON.stringify(s));
      m = parseInt(s.members, 16);
      document.getElementById('vname').value = s.name;
      var members = m & 0x3FF;
      var untag = (m >> 10) & 0x3FF;
      var pvid = parseInt(s.pvid, 16);
      console.log("PVID: ", pvid);
      for (let p = 1; p <= numPorts; p++) {
        var bit = physToLogPort[p-1];
        var isMember = (members >> bit) & 1;
        var isUntag = (untag >> bit) & 1;
        setC('t', p, isMember && !isUntag);
        setC('u', p, isMember && isUntag);
        setC('p', p, (pvid >> bit) & 1);
      }
    }
  };
  var v=document.getElementById('vid').value
  if (!v) {
    alert("Set VLAN ID first");
    return;
  }
  xhttp.open("GET", `/vlan.json?vid=${v}`, true);
  sendXHTTP(xhttp);
}

window.addEventListener("load", function() {
  update( () => {
    vlanForm();
    const interval = setInterval(update, 5000);
  });
});

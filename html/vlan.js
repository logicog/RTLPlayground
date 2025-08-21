var vlanInterval = Number();

function vlanForm() {
  if (!numPorts)
    return;
  clearInterval(vlanInterval);
  var t = document.getElementById('tPorts');
  var u = document.getElementById('uPorts');
  for (let i = 1; i <= numPorts; i++) {
    const d = document.createElement("div");
    d.classList.add("cbgroup");
    const l = document.createElement("label");
    l.innerHTML = "" + i;
    l.classList.add("cbgroup");
    const inp = document.createElement("input");
    inp.type = "checkbox";
    inp.id = "tport" + i;
    inp.setAttribute('onclick', `setC("u", ${i}, false);`);
    const o = document.createElement("img");
    if (pIsSFP[i - 1]) {
      o.src = "sfp.svg"; o.width ="60"; o.height ="60";
    } else {
      o.src = "port.svg"; o.width = "40"; o.height = "40";
    }
    l.appendChild(inp);
    l.appendChild(o);
    d.appendChild(l)
    t.appendChild(d);
    var d2=d.cloneNode(true);
    d2.children[0].children[0].id = "uport" + i;
    d2.children[0].children[0].setAttribute('onclick', `setC("t", ${i}, false);`);
    u.appendChild(d2);
  }
}

function setC(t, p, c)
{
  document.getElementById(t+'port'+p).checked=c;
}

function utClicked(t){
  for (let i = 1; i <= numPorts; i++) {
    setC('t', i, t); setC('u', i, !t);
  }
}

window.addEventListener("load", function() {
  vlanInterval = setInterval(vlanForm, 100);
});

function fetchVLAN() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("VLAN: ", JSON.stringify(s));
      m = parseInt(s.members, 16);
      for (let i = 1; i <= numPorts; i++) {
	setC('t', i, (m>>(10+i-1))&1);
	setC('u', i, (m>>(i-1))&1);
      }
    }
  };
  var v=document.getElementById('vid').value
  if (!v) {
    alert("Set VLAN ID first");
    return;
  }
  xhttp.open("GET", `/vlan.json?vid=${v}`, true);
  xhttp.send(); 
}

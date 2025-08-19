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
    inp.setAttribute('onclick', `pClicked(true, ${i});`);
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
    d2.children[0].children[0].setAttribute('onclick', `pClicked(false, ${i});`);
    u.appendChild(d2);
  }
}

function pClicked(t, n){
  if (t)
    document.getElementById('uport' + n).checked=false;
  else
    document.getElementById('tport' + n).checked=false;
}

function utClicked(t){
  console.log("CLICKED 1");
  for (let i = 1; i <= numPorts; i++) {
    document.getElementById('tport' + i).checked=t;
    document.getElementById('uport' + i).checked=!t;
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
      console.log("Members: ", m.toString(16));
      for (let i = 1; i <= numPorts; i++) {
        document.getElementById('tport' + i).checked=(m>>(16+i-1))&1;
        document.getElementById('uport' + i).checked=(m>>(i-1))&1;
      }
    }
  };
  var v=document.getElementById('vid').value
  if (!v) {
    alert("Please enter a VLAN ID first");
    return;
  }
  xhttp.open("GET", `/vlan.json?vid=${v}`, true);
  xhttp.send(); 
}

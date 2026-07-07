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
    alert(t('vlan_set_id_first'));
    return;
  }
  xhttp.open("GET", `/vlan.json?vid=${v}`, true);
  sendXHTTP(xhttp);
}

function portsToRange(mask, nPorts) {
  var parts = [];
  var start = -1, prev = -1;
  for (var p = 1; p <= nPorts; p++) {
    var bit = physToLogPort[p - 1];
    if ((mask >> bit) & 1) {
      if (start < 0) start = p;
      prev = p;
    } else {
      if (start >= 0) {
        parts.push(start === prev ? String(start) : start + '-' + prev);
        start = -1; prev = -1;
      }
    }
  }
  if (start >= 0)
    parts.push(start === prev ? String(start) : start + '-' + prev);
  return parts.length ? parts.join(',') : '-';
}

async function loadVlanTable() {
  var tbody = document.getElementById('vlanTableBody');
  if (!tbody) return;
  tbody.innerHTML = '';
  var resp;
  try { resp = await fetch('/vlanlist'); } catch(e) { return; }
  if (!resp.ok) return;
  var vlans = await resp.json();
  for (var i = 0; i < vlans.length; i++) {
    var v = vlans[i];
    var vresp;
    try { vresp = await fetch('/vlan.json?vid=' + v.id); } catch(e) { continue; }
    if (!vresp.ok) continue;
    var s = await vresp.json();
    var m = parseInt(s.members, 16);
    var members = m & 0x3FF;
    var untag   = ((m >> 10) & 0x3FF) & members;
    var tagged  = members & ~untag;
    var pvid    = parseInt(s.pvid, 16) & 0x3FF;
    var tr = document.createElement('tr');
    var td, a, btn;
    td = document.createElement('td');
    a = document.createElement('a');
    a.href = '#';
    a.textContent = v.id;
    (function(vid) {
      a.onclick = function(e) {
        e.preventDefault();
        document.getElementById('vid').value = vid;
        fetchVLAN();
      };
    })(v.id);
    td.appendChild(a); tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = v.name || ''; tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = portsToRange(members, numPorts); tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = portsToRange(tagged, numPorts); tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = portsToRange(untag, numPorts); tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = portsToRange(pvid, numPorts); tr.appendChild(td);
    td = document.createElement('td');
    if (v.id !== 1) {
      btn = document.createElement('button');
      btn.textContent = '✕';
      (function(vid) {
        btn.onclick = function() { deleteVlan(vid); };
      })(v.id);
      td.appendChild(btn);
    }
    tr.appendChild(td);
    tbody.appendChild(tr);
  }
}

function deleteVlan(id) {
  if (!confirm(t('vlan_delete_confirm') + id + '?')) return;
  fetch('/cmd', { method: 'POST', body: 'vlan ' + id + ' d' })
    .then(function() { refreshVlanViews(); })
    .catch(function(err) { console.error('Delete failed:', err); });
}

function refreshVlanViews() {
  loadVlanList();
  loadVlanTable();
}

function loadVlanList() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState !== 4) return;
    var sel = document.getElementById('vlanSelect');
    if (this.status !== 200) {
      sel.style.display = 'none';
      return;
    }
    var vlans = JSON.parse(this.responseText);
    if (!vlans.length) {
      sel.style.display = 'none';
      return;
    }
    sel.options.length = 1;
    for (var i = 0; i < vlans.length; i++) {
      var opt = document.createElement('option');
      opt.value = vlans[i].id;
      opt.text = vlans[i].name ? vlans[i].id + ' — ' + vlans[i].name : String(vlans[i].id);
      sel.appendChild(opt);
    }
  };
  xhttp.open('GET', '/vlanlist', true);
  sendXHTTP(xhttp);
}

window.addEventListener("load", function() {
  update( () => {
    vlanForm();
    refreshVlanViews();
    document.getElementById('vlanSelect').onchange = function() {
      document.getElementById('vid').value = this.value;
      fetchVLAN();
    };
    const interval = setInterval(update, 2000);
  });
});

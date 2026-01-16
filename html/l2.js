var l2GetInterval;
var l2Entries = [];
var l2CurrentEntry = 0;

function fillStats() {
  var tbl = document.getElementById('statstable');
  if (!numPorts)
    return;
  if (tbl.rows.length > 1) {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table Update row: " + i + " state " + pState[i] + " is " + linkS[pState[i] +1]);
      tbl.rows[i+1].cells[1].innerHTML = `${linkS[pState[i]+1]}`;
      tbl.rows[i+1].cells[2].innerHTML = `${txG[i]} pkts`;
      tbl.rows[i+1].cells[3].innerHTML = `${txB[i]} pkts`;
      tbl.rows[i+1].cells[4].innerHTML = `${rxG[i]} pkts`;
      tbl.rows[i+1].cells[5].innerHTML = `${rxB[i]} pkts`;
    }
  } else {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table row: " + i);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(`Port ${i+1}`));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${linkS[pState[i]+1]}`));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${txG[i]} pkts`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${txB[i]} pkts`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxG[i]} pkts`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxB[i]} pkts`));
    }
  }
}

function l2CMP(a, b)
{
  if (a.port < b.port)
    return -1;
  if (a.port > b.port)
    return 1;
  if (a.mac < b.mac)
    return -1;
  if (a.mac > b.mac)
    return 1;
  if (a.vlan < b.vlan)
    return -1;
  if (a.vlan > b.vlan)
    return 1;
  return 0;
}

function uniq(a) {
    return a.filter(function(item, pos, ary) {
        return !pos || item.idx != ary[pos - 1].idx;
    });
}

function delL2(idx) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var s = JSON.parse(xhttp.responseText);
      console.log("Entry deletion result: ", s.result);
    }
  };
  xhttp.open("GET", "/l2_del.json?idx=" + idx, true);
  xhttp.timeout = 1500; xhttp.send();
}

function fillL2(s)
{
  var tbl = document.getElementById('l2table');
  if (!s.length)
    return;
  s.sort(l2CMP);
  s = uniq(s);
  var s = s.map(function(e) { e.port = e.port != 9 ? e.port : "CPU"; return e; });
  console.log("L2: ", JSON.stringify(s));
  for (let i = 0; i < s.length; i++) {
    var e = s[i];
    console.log(i, e);
    if (tbl.rows[i+1]) {
      tbl.rows[i+1].cells[0].innerHTML = `${e.port}`;
      tbl.rows[i+1].cells[1].innerHTML = `${e.mac}`;
      tbl.rows[i+1].cells[2].innerHTML = `${e.vlan}`;
      tbl.rows[i+1].cells[4].innerHTML = '<button type="button" onclick="delL2(' + e.idx + ');">Delete</button>';
    } else {
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.innerHTML = `${e.port}`;
      td = tr.insertCell(); td.innerHTML = `${e.mac}`;
      td = tr.insertCell(); td.innerHTML = `${e.vlan}`;
      td = tr.insertCell(); td.innerHTML = `${e.type}`;
      td = tr.insertCell(); td.innerHTML = '<button type="button" onclick="delL2(' + e.idx + ');">Delete</button>';
    }
  }
  for (let i = tbl.rows.length - 1; i > s.length; i--)
    tbl.deleteRow(i);
  l2Entries = [];
}

function getL2() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var s = JSON.parse(xhttp.responseText);
      var s = s.map(function(e) { 
        e.vlan = parseInt(e.vlan, 16);
        e.idx = parseInt(e.idx, 16);
        e.type = e.type == "s" ? "static" : "learned";
        e.port = e.port == 9 ? 9 : logToPhysPort[e.port];
      return e;
    });
      l2Entries.push(...s);
      if (l2Entries >= 4096) {
        l2Entries = [];
        l2CurrentEntry = 0;
        clearInterval(l2GetInterval);
        return;
      }
      var w = 0;
      for (var i = l2Entries.length-1; i > 0; i--) {
        if (l2Entries[0].idx == l2Entries[i].idx) {
          w = 1;
          break;
        }
      }
      if (w) {
        l2CurrentEntry = 0; 
        fillL2(l2Entries);
      } else {
        l2CurrentEntry = s[s.length-1].idx + 1;
      }
    }
  };
  xhttp.open("GET", "/l2.json?idx=" + l2CurrentEntry, true);
  xhttp.timeout = 1500; xhttp.send();
}

window.addEventListener("load", function() {
  l2GetInterval = setInterval(getL2, 1000);
});


function fillStats() {
  var tbl = document.getElementById('statstable');
  if (!numPorts)
    return;
  if (tbl.rows.length > 1) {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table Update row: " + i + " state " + pState[i] + " is " + linkS[pState[i] +1]);
      tbl.rows[i+1].cells[1].innerHTML = linkText(pState[i]+1);
      tbl.rows[i+1].cells[2].innerHTML = `${txG[i]}` + t('common_pkts');
      tbl.rows[i+1].cells[3].innerHTML = `${txB[i]}` + t('common_pkts');
      tbl.rows[i+1].cells[4].innerHTML = `${rxG[i]}` + t('common_pkts');
      tbl.rows[i+1].cells[5].innerHTML = `${rxB[i]}` + t('common_pkts');
    }
  } else {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table row: " + i);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(t('common_port') + (i+1)));
      td = tr.insertCell(); td.appendChild(document.createTextNode(linkText(pState[i]+1)));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${txG[i]}` + t('common_pkts')));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${txB[i]}` + t('common_pkts')));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxG[i]}` + t('common_pkts')));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxB[i]}` + t('common_pkts')));
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
      if (JSON.parse(xhttp.responseText).result == 1) {
        l2All = l2All.filter(function(e) { return e.idx != idx; });
        renderL2();
      }
    }
  };
  xhttp.open("GET", "/l2_del.json?idx=" + idx, true);
  xhttp.timeout = 1500; xhttp.send();
}

var l2All = [];
const l2Cols = ['port', 'mac', 'vlan', 'type'];
var l2SortCol = 'port';
var l2SortDir = 1;

function l2Key(e, col) {
  if (col === 'port') {
    if (e.port === 'CPU') return Number.MAX_SAFE_INTEGER;
    if (e.lag) return 100 + e.lag;
    return Number(e.port);
  }
  if (col === 'vlan') return Number(e.vlan);
  return String(e[col]).toLowerCase();
}

function l2SortBy(col) {
  l2SortDir = (col === l2SortCol) ? -l2SortDir : 1;
  l2SortCol = col;
  renderL2();
}

function l2FilterChanged() { renderL2(); }

function renderL2() {
  var tbl = document.getElementById('l2table');
  if (!tbl) return;
  var f = {};
  l2Cols.forEach(function(c) {
    var el = document.getElementById('l2f_' + c);
    f[c] = el ? el.value.trim().toLowerCase() : '';
  });
  var rows = l2All.filter(function(e) {
    return l2Cols.every(function(c) {
      return !f[c] || String(e[c]).toLowerCase().indexOf(f[c]) !== -1;
    });
  });
  rows.sort(function(a, b) {
    var x = l2Key(a, l2SortCol), y = l2Key(b, l2SortCol);
    return (x < y ? -1 : x > y ? 1 : 0) * l2SortDir;
  });
  l2Cols.forEach(function(c) {
    var a = document.getElementById('l2a_' + c);
    if (a) a.textContent = (c === l2SortCol) ? (l2SortDir > 0 ? ' \u25b2' : ' \u25bc') : ' \u21c5';
  });
  paintL2(tbl, rows);
  var cnt = document.getElementById('l2count');
  if (cnt) cnt.textContent = rows.length + ' / ' + l2All.length;
}

function fillL2(s)
{
  var tbl = document.getElementById('l2table');
  if (!s.length)
    return;
  s.sort(l2CMP);
  s = uniq(s);
  l2All = s.map(function(e) {
    if (e.lag)
      e.port = 'LAG' + e.lag;
    return e;
  });
  renderL2();
}

function delL2Button(e)
{
  if (e.port == 'CPU')
    return '';
  return '<button type="button" onclick="delL2(' + e.idx + ');">' + t('l2_delete') + '</button>';
}

function paintL2(tbl, s)
{
  console.log("L2: ", JSON.stringify(s));
  for (let i = 0; i < s.length; i++) {
    var e = s[i];
    console.log(i, e);
    if (tbl.rows[i+1]) {
      tbl.rows[i+1].cells[0].innerHTML = `${e.port}`;
      tbl.rows[i+1].cells[1].innerHTML = `${e.mac}`;
      tbl.rows[i+1].cells[2].innerHTML = `${e.vlan}`;
      tbl.rows[i+1].cells[3].innerHTML = `${e.type}`;
      tbl.rows[i+1].cells[4].innerHTML = delL2Button(e);
    } else {
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.innerHTML = `${e.port}`;
      td = tr.insertCell(); td.innerHTML = `${e.mac}`;
      td = tr.insertCell(); td.innerHTML = `${e.vlan}`;
      td = tr.insertCell(); td.innerHTML = `${e.type}`;
      td = tr.insertCell(); td.innerHTML = delL2Button(e);
    }
  }
  for (let i = tbl.rows.length - 1; i > s.length; i--)
    tbl.deleteRow(i);
}

function getL2() {
  walkL2(function(entries, ok) {
    if (ok) {
      for (var i = 0; i < entries.length; i++)
        entries[i].type = entries[i].type == "s" ? t('l2_static') : t('l2_learned');
      fillL2(entries);
    }
    setTimeout(getL2, 1000);
  });
}

window.addEventListener("load", function() {
  update( () => {
    getL2();
    const interval = setInterval(update, 2000);
  });;
});


// PoE manager page. Polls /poe.json, which returns the PoE driver's already-normalized per-port
// status - one object per port: { port, admin, on, class, v, ma }. All chip-specific decoding
// (register layout, scaling, port mapping) lives in the firmware driver, so this page is a plain
// renderer: it never touches raw registers. Power (W) = v * ma is the only (universal) derivation.

function poeW(mw) {            // milliwatts -> "W.mmm"
  return Math.floor(mw / 1000) + "." + String(mw % 1000).padStart(3, "0");
}

function getPoe() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState != 4)
      return;
    const none = document.getElementById('poeNone');
    if (this.status == 401) { document.location = "/login.html"; return; }
    // Non-PoE machine: the /poe.json route is compiled out -> non-200; show the notice.
    if (this.status != 200) { if (none) none.style.display = ''; return; }
    let ports;
    try { const s = JSON.parse(xhttp.responseText); ports = Array.isArray(s) ? s : []; }
    catch (e) { if (none) none.style.display = ''; return; }
    if (none) none.style.display = 'none';

    var tbl = document.getElementById('poeTable');
    while (tbl.rows.length > 1) tbl.deleteRow(1);
    let totalMw = 0;
    for (const p of ports) {
      const admin = p.admin != 0;
      const on = p.on != 0;
      const cls = p["class"];
      const tr = tbl.insertRow();
      tr.insertCell().textContent = "Port " + p.port;
      tr.insertCell().textContent = admin ? "Enable" : "Disable";
      tr.insertCell().textContent = on ? "On" : "Off";
      if (on) {
        const mw = p.v * p.ma;
        totalMw += mw;
        tr.insertCell().textContent = (cls >= 0 && cls <= 8) ? ("Class " + cls) : "unknown";
        tr.insertCell().textContent = poeW(mw);
        tr.insertCell().textContent = p.v;
        tr.insertCell().textContent = p.ma;
      } else {
        tr.insertCell().textContent = "-";   // Class
        tr.insertCell().textContent = "-";   // Power (W)
        tr.insertCell().textContent = "-";   // Voltage (V)
        tr.insertCell().textContent = "-";   // Current (mA)
      }
      // Per-row enable/disable button (toggles the admin state of this port)
      const actBtn = document.createElement('input');
      actBtn.type = 'button';
      actBtn.className = 'poe-act';
      actBtn.value = admin ? 'Disable' : 'Enable';
      actBtn.onclick = function() { poePortSet(p.port, !admin, actBtn); };
      tr.insertCell().appendChild(actBtn);
      tr.classList.toggle('isOK', on);
      tr.classList.toggle('disabled', !admin);
    }
    document.getElementById('poeCons').value = poeW(totalMw);
  };
  xhttp.open("GET", "/poe.json", true);
  xhttp.timeout = 2000;
  sendXHTTP(xhttp);
}

async function poeCmd(cmd) {
  try {
    await fetch('/cmd', { method: 'POST', body: cmd });
  } catch (err) {
    console.error("cmd failed:", cmd, err);
  }
}

// Per-row enable/disable (the table's Action button).
async function poePortSet(port, enable, btn) {
  if (btn) { btn.disabled = true; btn.value = "…"; }
  await poeCmd("poe port " + port + " " + (enable ? "on" : "off"));
  setTimeout(getPoe, 800);
}

// Enable/disable ALL ports at once (the "Enable all / Disable all" buttons).
async function poeGlobal(on) {
  await poeCmd("poe global " + (on ? "on" : "off"));
  setTimeout(getPoe, 1500);   // global re-applies every port; give the chip a moment
}

window.addEventListener("load", function() {
  update(() => {
    setInterval(update, 2000);   // keep refreshing the port-status widget (fills its tooltips)
    getPoe();
    setInterval(getPoe, 2000);
  });
});

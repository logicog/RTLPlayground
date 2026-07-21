/* Spanning Tree page: full RSTP configuration + live status.
 *
 * Every control applies IMMEDIATELY on change (POST /cmd "stp ...") - there is
 * no per-row Apply. The refresh (2 s) repopulates controls from /stp.json;
 * a global dirty flag suppresses that between a change and its confirmation
 * so the refresh never reverts an edit in flight (same lesson as the LAG page).
 */

// STP port states as encoded in the ASIC's MSTP register (2 bits per port)
const STP_STATES = ["Disabled", "Blocking", "Learning", "Forwarding"];
const STP_ROLES  = ["-", "Root", "Designated", "Alternate"];

// stp_pflags bits (keep in sync with rtl837x_stp.h)
const PF_ENABLED = 1, PF_ADMEDGE = 2, PF_AUTOEDGE = 4, PF_BPDUGUARD = 8,
      PF_ROOTGUARD = 16, PF_FILTER = 32, PF_OPEREDGE = 64, PF_TRIPPED = 128;

var stpDirty = false;
var stpRows = 0;        // ports table built?

async function stpCmd(cmd) {
  stpDirty = true;
  try {
    await fetch('/cmd', { method: 'POST', body: cmd });
  } catch(err) {
    console.error(`Error: ${err}`);
  }
  stpDirty = false;
  fetchStp();
}

function sel(id, opts, onch) {
  const s = document.createElement("select");
  s.id = id;
  for (const [v, label] of opts) {
    const o = document.createElement("option");
    o.value = v; o.textContent = label;
    s.appendChild(o);
  }
  s.addEventListener("change", onch);
  return s;
}

function num(id, min, max, onch) {
  const n = document.createElement("input");
  n.type = "number"; n.id = id; n.min = min; n.max = max; n.style.width = "4em";
  n.addEventListener("change", onch);
  return n;
}

function buildPortsTable(ports) {
  const tbl = document.getElementById("stpPortsTbl");
  for (const p of ports) {
    const tr = tbl.insertRow();
    tr.insertCell().textContent = p.p;                     // Port
    tr.insertCell().id = "st_" + p.p;                      // State
    tr.insertCell().id = "role_" + p.p;                    // Role
    tr.insertCell().appendChild(sel("en_" + p.p,
      [["on","on"],["off","off"]],
      e => stpCmd("stp port " + p.p + " " + e.target.value)));
    tr.insertCell().appendChild(sel("edge_" + p.p,
      [["auto","auto"],["on","edge"],["off","off"]],
      e => stpCmd("stp port " + p.p + " edge " + e.target.value)));
    tr.insertCell().appendChild(num("cost_" + p.p, 0, 255,
      e => stpCmd("stp port " + p.p + " cost " + e.target.value)));
    tr.insertCell().appendChild(num("prio_" + p.p, 0, 240,
      e => stpCmd("stp port " + p.p + " prio " + e.target.value)));
    tr.insertCell().appendChild(sel("guard_" + p.p,
      [["none","none"],["bpdu","BPDU"],["root","Root"]],
      e => stpCmd("stp port " + p.p + " guard " + e.target.value)));
    tr.insertCell().appendChild(sel("filt_" + p.p,
      [["off","off"],["on","on"]],
      e => stpCmd("stp port " + p.p + " filter " + e.target.value)));
  }
  stpRows = ports.length;
}

function fetchStp() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      // textContent throughout: rootMac comes from received BPDUs
      // (remote-controlled), never render it as HTML
      if (!stpRows)
        buildPortsTable(s.ports);
      document.getElementById("stpStat").textContent = s.on
        ? (s.weRoot
            ? "This switch is the root bridge (priority 0x" + s.rootPrio + ") — topology changes: " + parseInt(s.tc, 16)
            : "Root bridge: 0x" + s.rootPrio + " / " + s.rootMac
              + " via port " + s.rootPort + " — path cost: 0x" + s.cost
              + " — topology changes: " + parseInt(s.tc, 16))
        : "";
      // live status columns always refresh
      for (const p of s.ports) {
        const trip = (p.f & PF_TRIPPED) ? " (guard!)" : "";
        document.getElementById("st_" + p.p).textContent =
          s.on ? STP_STATES[p.st] + trip : "-";
        document.getElementById("role_" + p.p).textContent =
          s.on ? STP_ROLES[p.role] + ((p.f & PF_OPEREDGE) ? " edge" : "") : "-";
      }
      if (stpDirty)          // an edit is in flight - do not revert controls
        return;
      document.getElementById("stpMode").value = s.on ? "on" : "off";
      document.getElementById("bPrio").value = s.prio;
      document.getElementById("bVer").value = s.rstp ? "rstp" : "stp";
      document.getElementById("bHello").value = s.hello;
      document.getElementById("bMaxage").value = s.maxage;
      document.getElementById("bFwd").value = s.fwd;
      document.getElementById("bTxhold").value = s.txhold;
      for (const p of s.ports) {
        document.getElementById("en_" + p.p).value = (p.f & PF_ENABLED) ? "on" : "off";
        document.getElementById("edge_" + p.p).value =
          (p.f & PF_ADMEDGE) ? "on" : ((p.f & PF_AUTOEDGE) ? "auto" : "off");
        document.getElementById("cost_" + p.p).value = p.cost;
        document.getElementById("prio_" + p.p).value = p.prio;
        document.getElementById("guard_" + p.p).value =
          (p.f & PF_BPDUGUARD) ? "bpdu" : ((p.f & PF_ROOTGUARD) ? "root" : "none");
        document.getElementById("filt_" + p.p).value = (p.f & PF_FILTER) ? "on" : "off";
      }
    }
  };
  xhttp.open("GET", `/stp.json`, true);
  sendXHTTP(xhttp);
}

async function stpSub() {
  const on = document.getElementById("stpMode").value === "on";
  await stpCmd(on ? "stp on" : "stp off");
}

window.addEventListener("load", function() {
  // bridge priority: 0-15 (x4096)
  const bp = document.getElementById("bPrio");
  for (let i = 0; i < 16; i++) {
    const o = document.createElement("option");
    o.value = i; o.textContent = (i * 4096) + (i === 8 ? " (default)" : "");
    bp.appendChild(o);
  }
  bp.addEventListener("change", e => stpCmd("stp prio " + e.target.value));
  document.getElementById("bVer")
    .addEventListener("change", e => stpCmd("stp version " + e.target.value));
  document.getElementById("bHello")
    .addEventListener("change", e => stpCmd("stp hello " + e.target.value));
  document.getElementById("bMaxage")
    .addEventListener("change", e => stpCmd("stp maxage " + e.target.value));
  document.getElementById("bFwd")
    .addEventListener("change", e => stpCmd("stp fwd " + e.target.value));
  document.getElementById("bTxhold")
    .addEventListener("change", e => stpCmd("stp txhold " + e.target.value));
  document.getElementById("stpMode")
    .addEventListener("change", () => { stpDirty = true; });

  update( () => {
    fetchStp();
    const interval = setInterval(update, 2000);
    const stpInt = setInterval(fetchStp, 2000);
  });
});

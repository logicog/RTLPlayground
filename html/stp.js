
const STP_STATES = ["Disabled", "Blocking", "Learning", "Forwarding"];
const STP_ROLES  = ["-", "Root", "Designated", "Alternate"];

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
  const stat = document.getElementById("stpStatTbl");
  for (const p of [...ports].sort((a, b) => a.p - b.p)) {
    const tr = tbl.insertRow();
    tr.insertCell().textContent = p.p;                     // Port
    tr.insertCell().appendChild(sel("en_" + p.p,
      [["on","Enable"],["off","Disable"]],
      e => stpCmd("stp port " + p.p + " " + e.target.value)));
    const pc = num("cost_" + p.p, 0, 200000000,
      e => stpCmd("stp port " + p.p + " cost " + e.target.value));
    pc.style.width = "7em";
    pc.title = "0 - 200000000 (0 = Auto)";
    tr.insertCell().appendChild(pc);
    const pr = sel("prio_" + p.p, [], 
      e => stpCmd("stp port " + p.p + " prio " + e.target.value));
    for (let v = 0; v <= 240; v += 16) {
      const o = document.createElement("option");
      o.value = v; o.textContent = v + (v === 128 ? " (default)" : "");
      pr.appendChild(o);
    }
    tr.insertCell().appendChild(pr);
    tr.insertCell().appendChild(sel("edge_" + p.p,
      [["auto","Auto"],["on","Enable"],["off","Disable"]],
      e => stpCmd("stp port " + p.p + " edge " + e.target.value)));
    tr.insertCell().appendChild(sel("filt_" + p.p,
      [["off","Disable"],["on","Enable"]],
      e => stpCmd("stp port " + p.p + " filter " + e.target.value)));
    tr.insertCell().appendChild(sel("guard_" + p.p,
      [["none","None"],["bpdu","BPDU"],["root","Root"]],
      e => stpCmd("stp port " + p.p + " guard " + e.target.value)));
    tr.insertCell().appendChild(sel("p2p_" + p.p,
      [["auto","Auto"],["on","Enable"],["off","Disable"]],
      e => stpCmd("stp port " + p.p + " p2p " + e.target.value)));

    const sr = stat.insertRow();
    sr.insertCell().textContent = p.p;
    for (const id of ["st","role","db","dp","dc","oe","op"])
      sr.insertCell().id = id + "_" + p.p;
  }
  stpRows = ports.length;
}

function bridgeSelf(s) {
  return fmtBridgeId((s.prio * 4096).toString(16).padStart(4, "0") + s.myMac);
}

function fmtBridgeId(h) {
  if (!h || h.length < 16) return "";
  const prio = parseInt(h.slice(0, 4), 16);
  const mac = h.slice(4).replace(/(..)(?=.)/g, "$1:");
  return prio + "-" + mac.toUpperCase();
}

function fetchStp() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      if (!stpRows)
        buildPortsTable(s.ports);
      document.getElementById("stpStat").textContent = s.on
        ? (s.weRoot
            ? "This switch (" + bridgeSelf(s) + ") is the root bridge — topology changes: "
              + parseInt(s.tc, 16)
            : "This switch: " + bridgeSelf(s)
              + " — root bridge: " + fmtBridgeId(s.rootPrio + s.rootMac)
              + " via port " + s.rootPort + " — path cost: " + parseInt(s.cost, 16)
              + " — topology changes: " + parseInt(s.tc, 16))
        : "";
      for (const p of s.ports) {
        const trip = (p.f & PF_TRIPPED) ? " (guard!)" : "";
        document.getElementById("st_" + p.p).textContent =
          s.on ? STP_STATES[p.st] + trip : "-";
        document.getElementById("role_" + p.p).textContent =
          s.on ? STP_ROLES[p.role] : "-";
        document.getElementById("db_" + p.p).textContent = s.on ? fmtBridgeId(p.db) : "-";
        document.getElementById("dp_" + p.p).textContent =
          s.on ? (parseInt(p.dp.slice(0, 2), 16) + "-" + parseInt(p.dp.slice(2), 16)) : "-";
        document.getElementById("dc_" + p.p).textContent = s.on ? parseInt(p.dc, 16) : "-";
        document.getElementById("oe_" + p.p).textContent =
          s.on ? ((p.f & PF_OPEREDGE) ? "True" : "False") : "-";
        document.getElementById("op_" + p.p).textContent = s.on ? (p.p2 == 2 ? "False" : "True") : "-";
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
        document.getElementById("cost_" + p.p).value = parseInt(p.pc, 16);
        document.getElementById("prio_" + p.p).value = p.prio;
        document.getElementById("p2p_" + p.p).value = ["auto","on","off"][p.p2];
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
  document.getElementById("stpStat").textContent = on
    ? "Enabling STP. The ports start blocked and take up to "
      + (2 * document.getElementById("bFwd").value)
      + " s to reach forwarding, and this page can stay silent until they do."
    : "Disabling STP.";
  await stpCmd(on ? "stp on" : "stp off");
}

window.addEventListener("load", function() {
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

/* ---- Spanning Tree (RSTP) section ---- */

// STP port states as encoded in the ASIC's MSTP register (2 bits per port)
const STP_STATES = ["Disabled", "Blocking", "Learning", "Forwarding"];

// "user is editing" flag: while set, the periodic refresh must not overwrite
// the mode dropdown (same pattern as the LAG page - without it the 2 s refresh
// silently reverts the user's choice before Apply).
var stpDirty = false;

function fetchStp() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      // textContent throughout: rootMac comes from received BPDUs
      // (remote-controlled), never render it as HTML
      if (!stpDirty)
        document.getElementById("stpMode").value = s.on ? "on" : "off";
      document.getElementById("stpStat").textContent = s.on
        ? (s.weRoot
            ? "This switch is the root bridge (priority 0x" + s.rootPrio + ")"
            : "Root bridge: 0x" + s.rootPrio + " / " + s.rootMac
              + " \u2014 path cost: 0x" + s.cost)
        : "";
      let t = "";
      if (s.on) {
        t = "port  state\n";
        for (const p of s.ports)
          t += String(p.p).padEnd(6) + STP_STATES[p.st] + "\n";
      }
      document.getElementById("stpPorts").textContent = t;
    }
  };
  xhttp.open("GET", `/stp.json`, true);
  sendXHTTP(xhttp);
}

async function stpSub() {
  const on = document.getElementById("stpMode").value === "on";
  try {
    await fetch('/cmd', { method: 'POST', body: on ? "stp on" : "stp off" });
  } catch(err) {
    console.error(`Error: ${err}`);
  }
  stpDirty = false;      // editing done - let the refresh show the truth
  fetchStp();
}

window.addEventListener("load", function() {
  document.getElementById("stpMode")
    .addEventListener("change", () => { stpDirty = true; });
});

window.addEventListener("load", function() {
  update( () => {
    fetchStp();
    const interval = setInterval(update, 2000);
    const stpInt = setInterval(fetchStp, 2000);
  });
});

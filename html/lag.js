var lagInterval = Number();

function lagForm() {
  if (!numPorts)
    return;
  for (let j=0; j < 4; j++) {
    var lag = "mLAG" + j
    console.log("Adding LAG " + lag)
    var m = document.getElementById(lag);
    for (let i = 1; i <= numPorts; i++) {
      const d = document.createElement("div");
      d.classList.add("cbgroup");
      const l = document.createElement("label");
      l.innerHTML = "" + i;
      l.classList.add("cbgroup");
      const inp = document.createElement("input");
      inp.type = "checkbox"; inp.setAttribute("class","psel");
      inp.id = "p_" + lag + "_" + i;
      const o = document.createElement("img");
      if (pIsSFP[i - 1]) {
        o.src = "sfp.svg"; o.width ="60"; o.height ="60";
      } else {
        o.src = "port.svg"; o.width = "40"; o.height = "40";
      }
      l.appendChild(inp); l.appendChild(o);
      d.appendChild(l)
      m.appendChild(d);
    }
  }
  fetchLag();
}

function setL(p, c){
  console.log("LAG setting: ", p, " to ", c);
  document.getElementById(p).checked=c;
}

// Candidate-port mask per LACP-mode LAG (0 = LAG is static), from /lacp.json.
// For a LACP LAG the checkboxes show the *candidate* ports (admin config), not
// the live trunk members - those converge over time and are shown in lagStatN.
var lacpCfg = [0, 0, 0, 0];

function fetchLag() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("LAG: ", JSON.stringify(s));
      for (let l = 0; l < 4; l++) {
        // LACP-managed LAG: checkboxes reflect lacpCfg (set by fetchLacp)
        let members = lacpCfg[l] ? lacpCfg[l] : parseInt(s[l].members, 2);
        let hash = parseInt(s[l].hash, 16);
        for (let i = 1; i <= numPorts; i++) {
          let p = i - 1;
          if (numPorts < 9)
            p = physToLogPort[p];
          setL("p_mLAG"+l+"_"+i, members & (1<<p));
        }
      }
    }
  };
  xhttp.open("GET", `/lag.json`, true);
  sendXHTTP(xhttp);
}
async function lagSub(l) {
  var cmd = "lag " + l;
  if (document.getElementById("lacp_m" + l).checked)
    cmd = cmd + " lacp";
  else if (lacpCfg[l])
    // LACP unchecked on a LACP-managed LAG: release it from LACP first;
    // the static member set (possibly empty) is programmed right after.
    await fetch('/cmd', { method: 'POST', body: "lag " + l + " lacp off" })
      .catch(err => console.error(`Error: ${err}`));
  for (let i = 1; i <= numPorts; i++) {
    if (document.getElementById("p_mLAG"+l+"_"+i).checked)
      cmd = cmd + ` ${i}`;
  }
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
  } catch(err) {
    console.error(`Error: ${err}`);
  }
  fetchLacp();
}

function fetchLacp() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      // textContent throughout: agg/psys carry bytes taken from received
      // LACPDUs (remote-controlled), never render them as HTML
      for (let l = 0; l < 4; l++) {
        const lg = s.lags[l];
        lacpCfg[l] = parseInt(lg.cfg, 16);
        document.getElementById("lacp_m" + l).checked = !!lacpCfg[l];
        document.getElementById("lagStat" + l).textContent = lacpCfg[l]
          ? ("LACP: aggregator " + (lg.aggValid ? lg.agg : "(negotiating)")
             + " — active members: 0x" + lg.members)
          : "";
      }
      let t = "";
      if (s.on) {
        t = "port  lag   actor  partner  rxstate  rx-count  partner-system\n";
        for (const p of s.ports) {
          if (p.lag == 255) continue;     // not part of any LACP LAG
          t += String(p.p).padEnd(6) + String(p.lag + 1).padEnd(6)
             + p.a.padEnd(7) + p.pt.padEnd(9)
             + String(p.rs).padEnd(9) + String(parseInt(p.rx, 16)).padEnd(10) + p.psys + "\n";
        }
      }
      document.getElementById("lacpPorts").textContent = t;
    }
  };
  xhttp.open("GET", `/lacp.json`, true);
  sendXHTTP(xhttp);
}

window.addEventListener("load", function() {
  update( () => {
    lagForm();
    fetchLacp();
    const interval = setInterval(update, 2000);
    const lacpInt = setInterval(fetchLacp, 2000);
  });
});

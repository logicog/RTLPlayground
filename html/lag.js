var lagInterval = Number();

function lagForm() {
  if (!numPorts)
    return;
  clearInterval(lagInterval);
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
window.addEventListener("load", function() {
  lagInterval = setInterval(lagForm, 200);
});

function fetchLag() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("LAG: ", JSON.stringify(s));
      for (let l = 0; l < 4; l++) {
        let members = parseInt(s[l].members, 2);
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
  xhttp.send();
}
async function lagSub(l) {
  var cmd = "lag " + l;
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
}

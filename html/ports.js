var mtus = new Int16Array(10);
var clicked = new Int8Array(10);
function createPortTable() {
  var tbl = document.getElementById('speedtable');
   if (tbl.rows.length <= 2 && numPorts) {
     clearInterval(pTableInterval);
     const sSelect = '<select name="speed_sel" id="speed_sel">'
      + '<option value="auto">Auto</option>'
      + '<option value="2g5">2500MBit/Full</option>'
      + '<option value="1g">1000MBit/Full</option>'
      + '<option value="100m full">100MBit/Full</option>'
      + '<option value="100m half">100MBit/Half</option>'
      + '<option value="10m full">10MBit/Full</option>'
      + '<option value="10m half">10MBit/Half</option>'
      + '</select>';
      const dSwitch = '<input type="checkbox" id="disable_port" onchange="portOnOff();">'
     for (let i = 1; i <= numPorts; i++) {
      if (pIsSFP[i-1])
        continue;
      console.log("Table row: " + i + "pState: " + pState[i-2]);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(`Port ${i}`));
      td = tr.insertCell(); td.innerHTML = linkS[pState[i] + 1];
      td = tr.insertCell(); td.innerHTML = sSelect.replaceAll("speed_sel", "speed_sel_" + i);
      td = tr.insertCell(); td.innerHTML = dSwitch.replaceAll("disable_port", "disable_port_" + i)
						  .replace("portOnOff()", "portOnOff(" + i + ")");
      var button = '<button type="button" style="margin: 0 0 0 24px" onclick="applySpeed(' + i + ');">Apply</button>';
      td = tr.insertCell();
      td.innerHTML = button;
    }
  }
  tbl = document.getElementById('mtutable');
  if (tbl.rows.length <= 2 && numPorts) {
     const mSelect = '<select name="mtu_sel" id="mtu_sel">'
      + '<option value="16383">16383</option>'
      + '<option value="1522">1522</option>'
      + '<option value="1536">1536</option>'
      + '<option value="1552">1552</option>'
      + '<option value="9216">9216</option>'
      + '</select>';
      var tr = tbl.insertRow();
      for (let i = 1; i <= numPorts; i++) {
        let td = tr.insertCell();
        if (pIsSFP[i-1])
          td.innerHTML = '<object type="image/svg+xml" data="sfp.svg", width="60"</object>'
        else
          td.innerHTML = '<object type="image/svg+xml" data="port.svg", width="40"</object>'
      }
      tr = tbl.insertRow();
      for (let i = 1; i <= numPorts; i++) {
        let td = tr.insertCell();
        td.innerHTML = mSelect.replaceAll("mtu_sel", "mtu_sel_" + i);
      }
      tr = tbl.insertRow();
      for (let i = 1; i <= numPorts; i++) {
        let td = tr.insertCell();
        td.innerHTML = '<button type="button" style="margin: 0 0 0 24px" onclick="applyMTU(' + i + ');">Apply</button>';
      }
  }
}

function updatePortTable() {
  console.log("updatePortTable called");
  var tbl = document.getElementById('speedtable');
  if (tbl.rows.length <= 2 || !numPorts)
    return;
  for (let i = 1; i <= numPorts ; i++) {
    if (pIsSFP[i-1])
      continue;
    tbl.rows[i].cells[1].innerHTML = `${linkS[pState[i-1]+1]}`;
    if (!clicked[i] && pState[i - 1] < 0) {
      document.getElementById('speed_sel_' + i).disabled = true;
      document.getElementById('disable_port_' + i).checked = true;
    }
  }
}

async function applySpeed(port) {
  var speed = document.getElementById('speed_sel_' + port).value;
  var disabled = document.getElementById('disable_port_' + port).checked;
  var cmd = "port " + port + " ";
  if (!disabled)
    cmd = cmd + speed;
  else
    cmd = cmd + "off";
  console.log("CMD: " + cmd);
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

async function portOnOff(p) {
  var disabled = document.getElementById('disable_port_' + p).checked;
  document.getElementById('speed_sel_' + p).disabled = disabled;
  clicked[p] = 1;
}

async function applyMTU(port) {
  var mtu = document.getElementById('mtu_sel_' + port).value;
  var cmd = "mtu " + port + " " + mtu;
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('MTU Completed!', response);
    getMTUs();
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

function getMTUs() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("MTUS: ", JSON.stringify(s));
      for (let i = 0; i < s.length; i++) {
        p = s[i];
	let n = p.portNum;
        mtus[n] = parseInt(p.mtu, 16);
        var mtu = document.getElementById('mtu_sel_' + n);
        if (!mtu)
          continue;
        mtu.value = mtus[n];
        clearInterval(pMTUInterval);
      }
    }
  };
  xhttp.open("GET", "/mtu.json", true);
  xhttp.timeout = 1500; xhttp.send();
}

window.addEventListener("load", function() {
  const updatePortTableInterval = setInterval(updatePortTable, 1000);
});

const pTableInterval = setInterval(createPortTable, 1000);
const pMTUInterval = setInterval(getMTUs, 1200);

var mtus = new Int16Array(10);
function createPortTable() {
  var tbl = document.getElementById('speedtable');
   if (tbl.rows.length <= 2 && numPorts) {
     clearInterval(pTableInterval);
     const sSelect = '<select name="speed_sel" id="speed_sel">'
      + '<option value="auto"> Auto</option>'
      + '<option value="2g5"> 2.5GBit</option>'
      + '<option value="1g"> 1000MBit</option>'
      + '<option value="100m"> 100MBit</option>'
      + '<option value="10m"> 10MBit</option>'
      + '</select>';
     const dSelect = '<select name="duplex_sel" id="duplex_sel">'
      + '<option value="any"> Any</option>'
      + '<option value="full"> Full</option>'
      + '<option value="half"> Half</option>'
      + '</select>';
     for (let i = 1; i <= numPorts; i++) {
      if (pIsSFP[i-1])
        continue;
      console.log("Table row: " + i + "pState: " + pState[i-2]);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(`Port ${i}`));
      td = tr.insertCell(); td.innerHTML = linkS[pState[i] + 1];
      td = tr.insertCell(); td.innerHTML = "Unknown";
      td = tr.insertCell(); td.innerHTML = sSelect.replaceAll("speed_sel", "speed_sel_" + i);
      td = tr.insertCell(); td.innerHTML = dSelect.replaceAll("duplex_sel", "duplex_sel_" + i);      
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
    var td = tbl.rows[i].cells[2];
    var adv = pAdvertised[i-1];
      switch(pAdvertised[i-1]) {
        case 0x20:
          td.innerHTML = "2.5GBit Full Duplex"; break;
        case 0x10:
          td.innerHTML = "1000MBit Full Duplex"; break;
        case 0x2f:
          td.innerHTML = "AUTO"; break;
        case 0xc:
          td.innerHTML = "100MBit"; break;
        case 8:
          td.innerHTML = "100MBit Full Duplex"; break;
        case 4:
          td.innerHTML = "100MBit Half Duplex"; break;
        case 3:
          td.innerHTML = "10MBit"; break;
        case 2:
          td.innerHTML = "10MBit Full Duplex"; break;
        case 1:
          td.innerHTML = "10MBit Half Duplex"; break;
        default:
          td.innerHTML = "None"; break;
      }
  }
}

async function applySpeed(port) {
  var speed = document.getElementById('speed_sel_' + port).value;
  var duplex = document.getElementById('duplex_sel_' + port).value;
  var cmd = "port " + port + " " + speed;
  console.log("port " + port, ", speed: " + speed, ", duplex: ", duplex);
  try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    console.log('Completed!', response);
    if (duplex != "any") {
      cmd = "port " + port + " duplex " + duplex;
      const response = await fetch('/cmd', {
        method: 'POST',
        body: cmd
      });
      console.log('Completed!', response);
    }
  } catch(err) {
    console.error(`Error: ${err}`);
  }
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

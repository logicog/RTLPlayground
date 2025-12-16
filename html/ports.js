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
    console.log("pAdvertised for " + i + " is " + adv);
      if (pAdvertised[i-1] == 0)
        td.innerHTML = "None";
      else if (adv == 0b100000)
        td.innerHTML = "2.5GBit Full Duplex";
      else if (adv == 0b010000)
        td.innerHTML = "1000MBit Full Duplex";
      else if (adv == 0b111111)
        td.innerHTML = "AUTO";
      else if (adv == 0b000011)
        td.innerHTML = "10MBit";
      else if (adv == 0b000100)
        td.innerHTML = "100MBit Half Duplex";
  }
}

async function applySpeed(port) {
  var speed = document.getElementById('speed_sel_' + port).value;
  var duplex = document.getElementById('duplex_sel_' + port).value;
  var cmd = "port " + port + " ";
  cmd = cmd + speed;
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

window.addEventListener("load", function() {
  const updatePortTableInterval = setInterval(updatePortTable, 1000);
});

const pTableInterval = setInterval(createPortTable, 1000);

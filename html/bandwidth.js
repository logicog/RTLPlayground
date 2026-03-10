const iLayout = '" type="number" maxlength="10" size="10"  onfocus="inputFocus(';
function createBW() {
  var tbl = document.getElementById('bwtable');
  const limit = '<input type="checkbox" id="limit_port" onchange="exec();">'
  if (tbl.rows.length <= 2  && numPorts) {
     console.log("CREATING TABLE ", tbl.rows.length);
     for (let i = 2; i < 2 + numPorts; i++) {
       const tr = tbl.insertRow();
       let td = tr.insertCell(); td.appendChild(document.createTextNode(`Port ${i-1}`));
       td = tr.insertCell();
       td.innerHTML = limit.replaceAll("limit_port", "ilimit_port_" + i).replace("exec()", "iClicked(" + i + ")");
       td = tr.insertCell();
       td.innerHTML = 'UNLIMITED';
       td = tr.insertCell();
       td.innerHTML = limit.replaceAll("limit_port", "fc_port_" + i).replace("exec()", "document.getElementById('bwapply_" + i + "').disabled=false;");
       td = tr.insertCell();
       td.innerHTML = limit.replaceAll("limit_port", "elimit_port_" + i).replace("exec()", "eClicked(" + i + ")");
       td = tr.insertCell();
       td.innerHTML = 'UNLIMITED';
       var button = '<button type="button" id="bwapply_' + i + '" style="margin: 0 0 0 24px" onclick="applyBandwidth(' + i + ');">Apply</button>';
       td = tr.insertCell();
       td.innerHTML = button;
       document.getElementById("bwapply_" + i).disabled = true;
    }
  }
}

function iClicked(i)
{
  document.getElementById("bwapply_" + i).disabled=false;
  var tbl = document.getElementById('bwtable');
  var tr = tbl.rows[i];
  if (!document.getElementById("ilimit_port_" + i).checked) {
    tr.cells[2].innerHTML = "UNLIMITED";
    document.getElementById("fc_port_" + i).disabled = true;
    document.getElementById("fc_port_" + i).checked = true;
  } else {
    tr.cells[2].innerHTML = '<input id="ibw_' + i + iLayout + i + ')" value="0"/>';
    document.getElementById("fc_port_" + i).disabled = false;
    document.getElementById("fc_port_" + i).checked = true;
  }
}

function eClicked(i)
{
  document.getElementById("bwapply_" + i).disabled=false;
  var tbl = document.getElementById('bwtable');
  var tr = tbl.rows[i];
  if (!document.getElementById("elimit_port_" + i).checked) {
    tr.cells[5].innerHTML = "UNLIMITED";
  } else {
    tr.cells[5].innerHTML = '<input id="ebw_' + i + iLayout + i + ')" value="0"/>';
  }
}

function inputFocus(i)
{
    document.getElementById("bwapply_" + i).disabled=false;
}

async function doCMD(cmd)
{
  console.log("Sending >" + cmd + "<");
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

async function applyBandwidth(i) {
  var tbl = document.getElementById('bwtable');
  var tr = tbl.rows[i];
  var cmd = "bw in " + (i-1) + " off";
  if (document.getElementById("ilimit_port_" + i).checked)
    cmd = 'bw in ' + (i-1) + ' ' + parseInt(document.getElementById("ibw_" + i).value).toString(16).padStart(4, "0");;
  doCMD(cmd);
  if (document.getElementById("ilimit_port_" + i).checked) {
    if (!document.getElementById("fc_port_" + i).checked)
      cmd = "bw in " + (i-1) + " drop";
    doCMD(cmd);
  }
  var cmd = "bw out " + (i-1) + " off";
  if (document.getElementById("elimit_port_" + i).checked)
    cmd = 'bw out ' + (i-1) + ' ' + parseInt(document.getElementById("ebw_" + i).value).toString(16).padStart(4, "0");;
  doCMD(cmd);
}

function getBW() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    console.log("IN getBW ");
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("BW: ", JSON.stringify(s));
      var tbl = document.getElementById('bwtable');
      if (tbl.rows.length > 2 && numPorts) {
        for (let i = 2; i < 2 + numPorts; i++) {
          p = s[i-2];
          let n = p.portNum;
          let tr = tbl.rows[n+1];
          if (!document.getElementById("bwapply_" + (n+1)).disabled)
            continue;
          console.log("Table Update row: " + i + " portNum is " + n + ", pState is " + pState[i-2] + ", row number is " + (n+1));
          let iBW = parseInt(p.iBW,16) * 16; let eBW = parseInt(p.eBW,16) * 16;
          document.getElementById("ilimit_port_" + (n+1)).checked = p.iLimited;
          document.getElementById("elimit_port_" + (n+1)).checked = p.eLimited;
          if (!p.iLimited) {
            tr.cells[2].innerHTML = "UNLIMITED";
          } else {
            tr.cells[2].innerHTML = '<input id="ibw_' + (n+1) + iLayout + (n+1) + ')" value="' + iBW +'"/>';
          }
          if (!p.eLimited) {
            tr.cells[5].innerHTML = "UNLIMITED";
          } else {
            tr.cells[5].innerHTML = '<input id="ebw_' + (n+1) + iLayout + (n+1) + ')" value="' + eBW +'"/>';
          }
          document.getElementById("fc_port_" + (n+1)).checked = p.iFC==1?true:false;
          document.getElementById("fc_port_" + (n+1)).disabled = p.iLimited==1?false:true;
        }
      }
    }
  };
  xhttp.open("GET", "/bandwidth.json", true);
  xhttp.timeout = 1500; sendXHTTP(xhttp);
}

window.addEventListener("load", function() {
  update( () => {
    createBW();
    getBW();
    const interval = setInterval(update, 2000);
  });
});

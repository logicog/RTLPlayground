function createEEE() {
  var tbl = document.getElementById('eeetable');
   if (tbl.rows.length <= 2  && numPorts) {
     clearInterval(createEEEInterval);
     console.log("CREATING TABLE ", tbl.rows.length);
     for (let i = 2; i < 2 + numPorts; i++) {
      console.log("Table row: " + i + "pState: " + pState[i-2]);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(`Port ${i-1}`));
      for (let j = 0; j < 7; j++) {
        td = tr.insertCell(); td.appendChild(document.createTextNode(" "));
      }
    }
  }
}

function getEEE() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("EEE: ", JSON.stringify(s));
      var tbl = document.getElementById('eeetable');
      if (tbl.rows.length > 2 && numPorts) {
        for (let i = 2; i < 2 + numPorts; i++) {
          p = s[i-2];
          let n = p.portNum;
          console.log("Table Update row: " + i + " portNum is " + n + ", pState is " + pState[i-2]);
          let tr = tbl.rows[n+1];
          if (!p.isSFP) {
            let eee = parseInt(p.eee,2); let lp = parseInt(p.eee_lp,2);
            tr.cells[1].innerHTML = `${eee&4?"ON":"OFF"}`; tr.cells[2].innerHTML = `${eee&2?"ON":"OFF"}`; tr.cells[3].innerHTML = `${eee&1?"ON":"OFF"}`;
            tr.cells[4].innerHTML = `${lp&4?"ON":"OFF"}`; tr.cells[5].innerHTML = `${lp&2?"ON":"OFF"}`; tr.cells[6].innerHTML = `${lp&1?"ON":"OFF"}`;
            tr.cells[7].innerHTML = `${p.active}`;
            tr.classList.toggle('disabled', pState[i-2] < 0); tr.classList.toggle('isNOK', !p.active); tr.classList.toggle('isOK', p.active);
          }
          tr.classList.toggle('isSFP', p.isSFP);
        }
      }
    }
  };
  xhttp.open("GET", "/eee.json", true);
  xhttp.timeout = 1500; xhttp.send();
}

window.addEventListener("load", function() {
  getEEE();
  const iCount = setInterval(getEEE, 2000);
});
const createEEEInterval = setInterval(createEEE, 1000);

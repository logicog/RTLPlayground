function fillStats() {
  var tbl = document.getElementById('statstable');
  if (tbl.rows.length > 1) {
    for (let i = 0; i < 6; i++) {
      console.log("Table Update row: " + i + " state " + pState[i] + " is " + linkS[pState[i] +1]);
      tbl.rows[i+1].cells[1].innerHTML = `${linkS[pState[i]+1]}`;
      tbl.rows[i+1].cells[2].innerHTML = `${txG[i]} Byte`;
      tbl.rows[i+1].cells[3].innerHTML = `${txB[i]} Byte`;
      tbl.rows[i+1].cells[4].innerHTML = `${rxG[i]} Byte`;
      tbl.rows[i+1].cells[5].innerHTML = `${rxB[i]} Byte`;
    }
  } else {
    for (let i = 0; i < 6; i++) {
      console.log("Table row: " + i);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(`Port ${i+1}`));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${linkS[pState[i]+1]}`));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${txG[i]} Byte`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${txB[i]} Byte`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxG[i]} Byte`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxB[i]} Byte`));
    }
  }
}

const stat = setInterval(fillStats, 500);

const mib_counters = [
  "Interface in Octets", 8,
  "", 0,
  "Interface out Octets", 8,
  "", 0,
  "Interface in Unicast Pkts", 8,
  "", 0,
  "Interface in Multicast Pkts", 8,
  "", 0,
  "Interface in Broadcast Pkts", 8,
  "", 0,
  "Interface out Unicast Pkts", 8, // 10
  "", 0,
  "Interface out Multicast Pkts", 8,
  "", 0,
  "Interface out Broadcast Pkts", 8,
  "", 0,
  "Interface out discards", 4,
  "802.1d Tp Port in discards", 4,
  "802.3 Single collision frames", 4,
  "802.3 Multi collision frames", 4,
  "802.3 Deferred transmissions", 4, // 20
  "802.3 Late collisions", 4,
  "802.3 Excessive collisions", 4,
  "802.3 Symbol errors", 4,
  "802.3 Control in unknown opcodes", 4,
  "802.3 In Pause frames", 4,
  "802.3 Out Pause frames", 4,
  "Ether drop events", 4,
  "TX Ether Broadcast Pkts", 4,
  "TX Ether Multicast Pkts", 4,
  "TX Ether CRC Align errors", 4, // 30
  "RX Ether CRC Align errors", 4,
  "TX Ether Undersized Pkts", 4,
  "RX Ether Undersized Pkts", 4,
  "TX Ether Oversized Pkts", 4,
  "RX Ether Oversized Pkts", 4,
  "TX Ether Fragments", 4,
  "RX Ether fragments", 4,
  "TX Ether Jabbers", 4,
  "RX Ether Jabbers", 4,
  "TX Ether Collisions", 4, // 40
  "TX Ether Pkts 640 Octets", 4,
  "RX Ether Pkts 640 Octets", 4,
  "TX Ether 65-127 Octets", 4,
  "RX Ether 65-127 Octets", 4,
  "TX Ether Pkts 128-255 Octets", 4,
  "RX Ether Pkts 128-255 Octets", 4,
  "TX Ether Pkts 256-511 Octets", 4,
  "RX Ether Pkts 256-511 Octets", 4,
  "TX Ether Pkts 512-1023 Octets", 4,
  "RX Ether Pkts 512-1023 Octets", 4, // 50
  "TX Ether Pkts 1024-1518 Octets", 4,
  "RX Ether Pkts 1024-1518 Octets", 4,
  "", 4,
  "RX Ether Undersized Drop Pkts", 4, // 54
  "TX Ether Pkts >1518 Octets", 4,
  "RX Ether Pkts >1518 Octets", 4,
  "TX Ether Pkts too large", 4,
  "RX Ether Pkts too large", 4,
  "TX Ether Flexible Octets Set 1", 4,
  "RX Ether Flexible Octets Set 1", 4,// 60
  "TX Ether Flexible Octets CRC Set 1", 4,
  "RX Ether Flexible Octets CRC Set 1", 4,
  "TX Ether Flexible Octets Set 0", 4,
  "RX Ether Flexible Octets Set 0", 4,
  "TX Ether Flexible Octets CRC Set 0", 4,
  "RX Ether Flexible Octets CRC Set 0", 4,
  "Lenth Field Errors", 4,
  "False Carriers", 4,
  "Undersized Octets", 4,
  "Framing Errors", 4, // 70
  "", 4,
  "RX MAC Discards", 4, // 72
  "RX MAC IPG Short Drop", 4,
  "", 4,
  "802.1d TP Learned Entry Discards", 4, // 75
  "Egress Queue 7 Dropped Pkts", 4,
  "Egress Queue 6 Dropped Pkts", 4,
  "Egress Queue 5 Dropped Pkts", 4,
  "Egress Queue 4 Dropped Pkts", 4,
  "Egress Queue 3 Dropped Pkts", 4, // 80
  "Egress Queue 2 Dropped Pkts", 4,
  "Egress Queue 1 Dropped Pkts", 4,
  "Egress Queue 0 Dropped Pkts", 4,
  "Egress Queue 7 Out Pkts", 4,
  "Egress Queue 6 Out Pkts", 4,
  "Egress Queue 5 Out Pkts", 4,
  "Egress Queue 4 Out Pkts", 4,
  "Egress Queue 3 Out Pkts", 4,
  "Egress Queue 2 Out Pkts", 4,
  "Egress Queue 1 Out Pkts", 4, // 90
  "Egress Queue 0 Out Pkts", 4,
  "TX Good Counter", 8,
  "", 0,
  "RX Good Counter", 8,
  "", 0,
  "RX Error Counter", 4,
  "TX Error Counter", 4,
  "TX Good Counter PHY", 8,
  "", 0,
  "RX Good Counter PHY", 8, // 100
  "", 0,
  "RX Error Counter PHY", 4,
  "TX Error Counter PHY", 4
];


function getCounters(port) {
  var xhttp = new XMLHttpRequest();
  const popup = document.getElementById('popup');
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("Counters: ", JSON.stringify(s));
      const ptext = document.getElementById('popup_text');
      var t = "<table style='width:100%'> <tr> <th>Counter</th> <th>Value</th> <th>Counter</th> <th>Value</th></tr> <tr>";
      console.log("Counter 0: ", BigInt(s[0]).toString(), " length: ", s.length);
      var c = 0;
      for (i = 0; i < mib_counters.length; i += 4) {
        console.log(i, " ", mib_counters[i], ": ", mib_counters[i+1]);
        if (mib_counters[i] == "" && mib_counters[i + 1] == 8) {
          console.log("c " + i + ": continue");
          continue;
        }
        var count = BigInt(s[i/4]);
        if (mib_counters[i+1] == 8) {
          t += "<td>" + mib_counters[i] + "</td><td>" + count.toString() + "</td>";
          c += 1;
        } else if (mib_counters[i+1] == 4) {
          if (mib_counters[i] != "") {
            t += "<td>" + mib_counters[i] + "</td><td>" + (count >> 32n).toString() + "</td>";
            c += 1;
          }
          if (c == 2) {
            t += "</tr> <tr>";
            c = 0;
          }
          if (mib_counters[i+2] != "") {
            t += "<td>" + mib_counters[i+2] + "</td><td>" + (count & 4294967295n).toString() + "</td>";
            c += 1;
          }
        }
        if (c == 2) {
          t += "</tr> <tr>";
          c = 0;
        }
      }
      ptext.innerHTML = t + "</tr></table>";
      popup.style.display = 'flex';
    }
  };
  xhttp.open("GET", "/counters.json?port=" + port, true);
  xhttp.timeout = 1500; xhttp.send();
}


function fillStats() {
  var tbl = document.getElementById('statstable');
  if (!numPorts)
    return;
  if (tbl.rows.length > 1) {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table Update row: " + i + " state " + pState[i] + " is " + linkS[pState[i] +1]);
      tbl.rows[i+1].cells[1].innerHTML = `${linkS[pState[i]+1]}`;
      tbl.rows[i+1].cells[2].innerHTML = `${txG[i]} pkts`;
      tbl.rows[i+1].cells[3].innerHTML = `${txB[i]} pkts`;
      tbl.rows[i+1].cells[4].innerHTML = `${rxG[i]} pkts`;
      tbl.rows[i+1].cells[5].innerHTML = `${rxB[i]} pkts`;
    }
  } else {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table row: " + i);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(`Port ${i+1}`));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${linkS[pState[i]+1]}`));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${txG[i]} pkts`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${txB[i]} pkts`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxG[i]} pkts`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxB[i]} pkts`));
      var button = '<button type="button" style="margin: 0 0 0 24px" onclick="getCounters(' + i + ');">Show</button>';
      td = tr.insertCell(); td.innerHTML = button;
    }
  }
}

const stat = setInterval(fillStats, 1000);

const popup = document.getElementById('popup');
const closePopup = document.getElementById('closePopup');
closePopup.addEventListener('click', () => {
  popup.style.display = 'none';
});
window.addEventListener('click', (event) => {
  if (event.target === popup) {
    popup.style.display = 'none';
  }
});

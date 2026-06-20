const mib_counters = [
  "接口入方向字节", 8,
  "", 0,
  "接口出方向字节", 8,
  "", 0,
  "接口入方向单播包", 8,
  "", 0,
  "接口入方向组播包", 8,
  "", 0,
  "接口入方向广播包", 8,
  "", 0,
  "接口出方向单播包", 8, // 10
  "", 0,
  "接口出方向组播包", 8,
  "", 0,
  "接口出方向广播包", 8,
  "", 0,
  "接口出方向丢弃", 4,
  "802.1D TP 端口入方向丢弃", 4,
  "802.3 单次冲突帧", 4,
  "802.3 多次冲突帧", 4,
  "802.3 延迟发送", 4, // 20
  "802.3 晚期冲突", 4,
  "802.3 过多冲突", 4,
  "802.3 符号错误", 4,
  "802.3 未知控制操作码入方向", 4,
  "802.3 入方向 Pause 帧", 4,
  "802.3 出方向 Pause 帧", 4,
  "以太网丢弃事件", 4,
  "TX 以太网广播包", 4,
  "TX 以太网组播包", 4,
  "TX 以太网 CRC/对齐错误", 4, // 30
  "RX 以太网 CRC/对齐错误", 4,
  "TX 以太网过短包", 4,
  "RX 以太网过短包", 4,
  "TX 以太网超长包", 4,
  "RX 以太网超长包", 4,
  "TX 以太网碎片", 4,
  "RX 以太网碎片", 4,
  "TX 以太网 Jabber 帧", 4,
  "RX 以太网 Jabber 帧", 4,
  "TX 以太网冲突", 4, // 40
  "TX 以太网 640 字节包", 4,
  "RX 以太网 640 字节包", 4,
  "TX 以太网 65-127 字节包", 4,
  "RX 以太网 65-127 字节包", 4,
  "TX 以太网 128-255 字节包", 4,
  "RX 以太网 128-255 字节包", 4,
  "TX 以太网 256-511 字节包", 4,
  "RX 以太网 256-511 字节包", 4,
  "TX 以太网 512-1023 字节包", 4,
  "RX 以太网 512-1023 字节包", 4, // 50
  "TX 以太网 1024-1518 字节包", 4,
  "RX 以太网 1024-1518 字节包", 4,
  "", 4,
  "RX 以太网过短丢弃包", 4, // 54
  "TX 以太网 >1518 字节包", 4,
  "RX 以太网 >1518 字节包", 4,
  "TX 以太网过大包", 4,
  "RX 以太网过大包", 4,
  "TX 以太网灵活字节集合 1", 4,
  "RX 以太网灵活字节集合 1", 4,// 60
  "TX 以太网灵活字节 CRC 集合 1", 4,
  "RX 以太网灵活字节 CRC 集合 1", 4,
  "TX 以太网灵活字节集合 0", 4,
  "RX 以太网灵活字节集合 0", 4,
  "TX 以太网灵活字节 CRC 集合 0", 4,
  "RX 以太网灵活字节 CRC 集合 0", 4,
  "长度字段错误", 4,
  "假载波", 4,
  "过短字节", 4,
  "成帧错误", 4, // 70
  "", 4,
  "RX MAC 丢弃", 4, // 72
  "RX MAC IPG 过短丢弃", 4,
  "", 4,
  "802.1D TP 已学习条目丢弃", 4, // 75
  "出方向队列 7 丢弃包", 4,
  "出方向队列 6 丢弃包", 4,
  "出方向队列 5 丢弃包", 4,
  "出方向队列 4 丢弃包", 4,
  "出方向队列 3 丢弃包", 4, // 80
  "出方向队列 2 丢弃包", 4,
  "出方向队列 1 丢弃包", 4,
  "出方向队列 0 丢弃包", 4,
  "出方向队列 7 发出包", 4,
  "出方向队列 6 发出包", 4,
  "出方向队列 5 发出包", 4,
  "出方向队列 4 发出包", 4,
  "出方向队列 3 发出包", 4,
  "出方向队列 2 发出包", 4,
  "出方向队列 1 发出包", 4, // 90
  "出方向队列 0 发出包", 4,
  "TX 正常计数", 8,
  "", 0,
  "RX 正常计数", 8,
  "", 0,
  "RX 错误计数", 4,
  "TX 错误计数", 4,
  "PHY TX 正常计数", 8,
  "", 0,
  "PHY RX 正常计数", 8, // 100
  "", 0,
  "PHY RX 错误计数", 4,
  "PHY TX 错误计数", 4
];


function getCounters(port) {
  var xhttp = new XMLHttpRequest();
  const popup = document.getElementById('popup');
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("Counters: ", JSON.stringify(s));
      const ptext = document.getElementById('popup_text');
      var t = "<table style='width:100%'> <tr> <th>计数器</th> <th>值</th> <th>计数器</th> <th>值</th></tr> <tr>";
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
  xhttp.timeout = 1500; sendXHTTP(xhttp);
}


function fillStats() {
  var tbl = document.getElementById('statstable');
  if (!numPorts)
    return;
  if (tbl.rows.length > 1) {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table Update row: " + i + " state " + pState[i] + " is " + linkS[pState[i] +1]);
      tbl.rows[i+1].cells[2].innerHTML = `${linkS[pState[i]+1]}`;
      tbl.rows[i+1].cells[3].innerHTML = `${txG[i]} 包`;
      tbl.rows[i+1].cells[4].innerHTML = `${txB[i]} 包`;
      tbl.rows[i+1].cells[5].innerHTML = `${rxG[i]} 包`;
      tbl.rows[i+1].cells[6].innerHTML = `${rxB[i]} 包`;
    }
  } else {
    for (let i = 0; i < numPorts; i++) {
      console.log("Table row: " + i);
      const tr = tbl.insertRow();
      let td = tr.insertCell(); td.appendChild(document.createTextNode(`端口 ${i+1}`));
      let portName = portNames[physToLogPort[i]] || '';
      td = tr.insertCell(); td.appendChild(document.createTextNode(portName));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${linkS[pState[i]+1]}`));
      td = tr.insertCell(); td.appendChild(document.createTextNode(`${txG[i]} 包`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${txB[i]} 包`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxG[i]} 包`));
      td = tr.insertCell();td.appendChild(document.createTextNode(`${rxB[i]} 包`));
      var button = '<button type="button" style="margin: 0 0 0 24px" onclick="getCounters(' + i + ');">查看</button>';
      td = tr.insertCell(); td.innerHTML = button;
    }
  }
}

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

window.addEventListener("load", function() {
  update( () => {
    update();
    fillStats();
    const stat = setInterval(fillStats, 1000);
    const interval = setInterval(update, 2000);
  });
});

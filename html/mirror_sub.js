async function mirrorSub() {
  var cmd = "mirror ";
  var mp=document.getElementById('mp').value
  if (!mp) {
    alert("请先设置镜像目标端口");
    return;
  }
  document.getElementById(mirrors[0]+mp).checked=false;document.getElementById(mirrors[1]+mp).checked=false;
  cmd = cmd + mp;
  for (let i = 1; i <= numPorts; i++) {
    if (document.getElementById(mirrors[0] + i).checked && document.getElementById(mirrors[1] + i).checked)
      cmd = cmd + ` ${i}`;
    else if (document.getElementById(mirrors[0] + i).checked)
      cmd = cmd + ` ${i}t`;
    else if (document.getElementById(mirrors[1] + i).checked)
      cmd = cmd + ` ${i}r`;
  }
  if (cmd.length < 10) {
    alert("请选择被镜像端口");
    return;
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
async function mirrorDel() {
  var cmd = "mirror off";
try {
    const response = await fetch('/cmd', {
      method: 'POST',
      body: cmd
    });
    location.reload();
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}

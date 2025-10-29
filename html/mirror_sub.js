async function mirrorSub() {
  var cmd = "mirror ";
  var mp=document.getElementById('mp').value
  if (!mp) {
    alert("Set Mirroring Port first");
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
    alert("Select Mirrored Ports");
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

async function vlanSub() {
  var commands = [];
  var cmd = "vlan ";
  var v=document.getElementById('vid').value
  if (!v) {
    alert("Set VLAN ID first");
    return;
  }
  cmd = cmd + v;
  if (document.getElementById('vname').value)
    cmd = cmd + ' ' + document.getElementById('vname').value;
  for (let i = 1; i <= numPorts; i++) {
    if (document.getElementById('tport' + i).checked)
      cmd = cmd + ` ${i}t`;
    else if (document.getElementById('uport' + i).checked)
      cmd = cmd + ` ${i}`;
  }
  commands.push(cmd);
  for (let i = 1; i <= numPorts; i++) {
    if (document.getElementById('pport' + i).checked)
      commands.push(`pvid ${i} ${v}`);
  }
  try {
    for (let c of commands) {
      const response = await fetch('/cmd', {
        method: 'POST',
        body: c
      });
      console.log('Completed!', response);
    }
  } catch(err) {
    console.error(`Error: ${err}`);
  }
}


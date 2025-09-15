async function vlanSub() {
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


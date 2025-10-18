async function eeeSub(port, enable) {
  var cmd = "eee ";
  if (enable)
    cmd = cmd + "on";
  else
    cmd = cmd + "off";
  console.log("eeeSub port " + port, ", value " + enable);
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


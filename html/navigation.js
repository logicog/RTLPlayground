document.getElementById('sidebar').innerHTML =
 "<ul><li><a href='index.html'>Overview</a></li>"
 + "<li><a href='ports.html'>Port Configuration</a></li>"
 + "<li><a href='stat.html'>Port Statistics</a></li>"
 + "<li><a href='vlan.html'>VLAN</a></li>"
 + "<li><a href='l2.html'>L2 Configuration</a></li>"
 + "<li><a href='mirror.html'>Mirroring</a></li>"
 + "<li><a href='lag.html'>Link Aggregation</a></li>"
 + "<li><a href='eee.html'>EEE</a></li>"
 + "<li><a href='bandwidth.html'>Bandwidth Limits</a></li>"
 + "<li><a href='system.html'>System Settings</a></li>"
 + "<li><a href='update.html'>Firmware Update</a></li></ul>";

// poe.html ships in every build, but the PoE code (and /poe.json) is compiled in only on PoE
// machines. Show the PoE menu entry only when the firmware has it - i.e. when /poe.json answers.
fetch('/poe.json').then(function(r) {
  if (!r.ok) return;
  var sys = document.querySelector("#sidebar a[href='system.html']").parentNode;
  sys.insertAdjacentHTML('beforebegin', "<li><a href='poe.html'>PoE</a></li>");
}).catch(function() {});

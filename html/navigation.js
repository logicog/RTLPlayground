document.getElementById('sidebar').innerHTML =
 "<ul><li><a href='index.html' data-i18n='nav_overview'>Overview</a></li>"
 + "<li><a href='ports.html' data-i18n='nav_port_config'>Port Configuration</a></li>"
 + "<li><a href='stat.html' data-i18n='nav_port_stat'>Port Statistics</a></li>"
 + "<li><a href='vlan.html' >VLAN</a></li>"
 + "<li><a href='l2.html' data-i18n='nav_l2'>L2 Configuration</a></li>"
 + "<li><a href='mirror.html' data-i18n='nav_mirror'>Mirroring</a></li>"
 + "<li><a href='lag.html' data-i18n='nav_lag'>Link Aggregation</a></li>"
 + "<li><a href='eee.html' data-i18n='nav_eee'>EEE</a></li>"
 + "<li><a href='bandwidth.html' data-i18n='nav_bandwidth'>Bandwidth Limits</a></li>"
 + "<li><a href='system.html' data-i18n='nav_system'>System Settings</a></li>"
 + "<li><a href='update.html' data-i18n='nav_fw_update'>Firmware Update</a></li></ul>";

document.addEventListener('DOMContentLoaded', function() {
  var links = document.querySelectorAll('#sidebar a[data-i18n]');
  links.forEach(function(el) {
    var key = el.getAttribute('data-i18n');
    if (key) el.textContent = t(key);
  });
});

// Add PoE menu item if the device supports it. The capability flag is cached in localStorage
// by main_info.js.
function addPoeMenu() {
  if (document.querySelector("#sidebar a[href='poe.html']")) return; // already present
  var sys = document.querySelector("#sidebar a[href='system.html']").parentNode;
  sys.insertAdjacentHTML('beforebegin', "<li><a href='poe.html'>PoE</a></li>");
}

var poeCap = localStorage.getItem('cap_poe');
if (poeCap === null) {
  // Not cached yet (fresh browser, or a tab opened before the Overview page ran):
  // wait for main_info.js in another tab to write it.
  window.addEventListener('storage', function(e) {
    if (e.key === 'cap_poe' && e.newValue === '1')
      addPoeMenu();
  }, { once: true });
} else if (poeCap === '1') {
  addPoeMenu();
}

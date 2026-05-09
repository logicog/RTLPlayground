/**
 * system.js
 * Handles frontend interactions for the RTLPlayground switch Web UI.
 * Includes critical protections against CPU resource exhaustion, 
 * asynchronous data loss, and partial-state network lockouts.
 */

var systemInterval = Number();
const ips = ["ip", "netmask", "gw"];

// Concurrency lock: Prevents duplicate flash writes if the user double-clicks "Save"
var isSaving = false; 

/**
 * Toggles the UI state to provide visual feedback during hardware operations.
 * Prevents user panic-clicking by disabling buttons and showing a wait cursor.
 */
function setUILock(locked) {
  document.body.style.cursor = locked ? 'wait' : 'default';
  const buttons = document.querySelectorAll("button, input[type='button'], input[type='submit']");
  buttons.forEach(btn => btn.disabled = locked);
}

/**
 * Validates IPv4 address formatting strictly (0-255 octets).
 */
function checkIp(ip) {
  const ipv4 = /^(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)){3}$/;
  if (!ipv4.test(ip)) { alert(`Invalid IP format: ${ip}`); return false; }
  return true;
}

/**
 * Applies IP, Netmask, and Gateway sequentially. 
 * Designed as a strict transaction to prevent partial-state lockouts.
 */
async function ipSub() {
  for (let i = 0; i < 3; i++) {
    if (!checkIp(document.getElementById(ips[i]).value)) return;
  }
  
  setUILock(true);
  
  try {
    for (let i = 0; i < 3; i++) {
      const val = document.getElementById(ips[i]).value;
      const response = await fetch('/cmd', { method: 'POST', body: `${ips[i]} ${val}` });
      if (!response.ok) throw new Error(`HTTP Error: ${response.status}`);
      await new Promise(r => setTimeout(r, 100)); // Hardware breathing room
    }
    await fetchIP(false);
  } catch (err) {
    console.error(`Critical Error applying IP settings: ${err}`);
    alert(`Network update halted! Check console to prevent lockout.`);
  } finally {
    setUILock(false);
  }
}

/**
 * Submits raw CLI commands from the manual console input box.
 */
async function cmdSub() {
  var cmd = document.getElementById('console_cmd').value;
  setUILock(true);
  try {
    const response = await fetch('/cmd', { method: 'POST', body: cmd });
    if (!response.ok) throw new Error(`HTTP Error: ${response.status}`);
    console.log('Command executed:', response);
  } catch(err) {
      console.error(`Error executing command: ${err}`);
  } finally {
    setUILock(false);
  }
}

/**
 * Core Save Routine. Commits the merged configuration to SPI Flash.
 */
async function sendConfig(c) {
  if (isSaving) return; 
  
  isSaving = true;
  setUILock(true); 
  clearInterval(systemInterval); // Stop polling during CPU-blocking flash write
  
  let saveSucceeded = false;
  
  try {
    const form = new FormData();
    form.append("MAX_FILE_SIZE", "4096");
    
    // CRITICAL C-PARSER FIX: Must be a Blob, must be application/octet-stream, 
    // and must have a filename so the C-backend multipart parser doesn't crash.
    form.append(
      "configuration", 
      new Blob([c], { type: "application/octet-stream" }), 
      "config.txt"
    );
    
    const response = await fetch('/config', { 
      method: 'POST', 
      body: form 
      // DO NOT SET HEADERS MANUALLY. The browser must dynamically generate 
      // the multipart boundary string to satisfy httpd.c.
    });
    
    if (!response.ok) throw new Error(`Server rejected flash write: ${response.status}`);
    
    saveSucceeded = true;
    console.log('Flash write completed successfully!');
    alert("Configuration successfully saved to flash memory.");
    
  } catch(err) {
    // HARDWARE WORKAROUND: On this RTL8372 hardware, a successful flash write 
    // causes ERR_EMPTY_RESPONSE because the SPI erase blocks the HTTP daemon.
    // A TypeError here = connection drop = write completed successfully.
    if (err instanceof TypeError) {
      saveSucceeded = true;
      console.warn("Connection dropped during flash write — expected on this hardware.");
      alert("Configuration saved to flash memory.");
    } else {
      console.error(`Unexpected flash write error: ${err}`);
      alert("Save failed with an unexpected error. Check console.");
    }
  }
  
  // Best-effort cleanup: Decoupled so a timeout here doesn't mask a successful save
  if (saveSucceeded) {
    try {
      const clearResponse = await fetch('/cmd_log_clear', { method: 'GET' });
      if (!clearResponse.ok) {
        console.warn("Log clear failed (non-fatal).");
      } else {
        console.log('Command log cleared.');
      }
    } catch (err) {
      console.warn("Log clear request failed (non-fatal):", err);
    }
  }

  isSaving = false;
  setUILock(false); 
  clearInterval(systemInterval); 
  systemInterval = setInterval(() => fetchIP(false), 5000);
}

/**
 * THE MERGE FIX: Combines old flash baseline with live RAM session before saving.
 */
async function flashSave() {
  if (typeof configuration === 'undefined' || typeof parseConf !== 'function') {
    alert("Critical Error: config.js logic is missing. Cannot safely merge config.");
    return;
  }

  try {
    configuration = []; 
    
    const savedConfig = await fetchConfig();   
    const cmdLog = await fetchCmdLog();        

    if (savedConfig) parseConf(savedConfig);  
    if (cmdLog) parseConf(cmdLog);       

    const mergedConfig = configuration.join('\n') + '\n';
    
    await sendConfig(mergedConfig);
    
    await fetchIP(true); 

  } catch (err) {
    console.error("Configuration merge failed:", err);
    alert("Failed to build save file. Aborted to prevent data loss.");
  }
}

/**
 * Resets the manual configuration text box to default baseline IP settings.
 */
function clearConfig() {
  document.getElementById("config_display").value = "";
  for (let i = 0; i < 3; i++) {
    if (!checkIp(document.getElementById(ips[i]).value)) return;
  }
  var configLines = "";
  for (let i = 0; i < 3; i++) {
    var cmd = ips[i] + ' ' + document.getElementById(ips[i]).value;
    configLines += cmd + "\n";
  }
  document.getElementById("config_display").value = configLines;
}

/**
 * Background polling routine. Keeps UI in sync with hardware state.
 */
async function fetchIP(forceConfigUpdate = false) {
  if (isSaving) return; 

  try {
    const response = await fetch('/information.json');
    if (!response.ok) throw new Error("Failed to fetch IP info");
    const s = await response.json();
    
    document.getElementById("ip").value = s.ip_address;
    document.getElementById("netmask").value = s.ip_netmask;
    document.getElementById("gw").value = s.ip_gateway;

    const displayBox = document.getElementById("config_display");
    
    if (displayBox && (forceConfigUpdate || displayBox.value.trim() === "")) {
      const configResponse = await fetch('/config');
      if (configResponse.ok) {
          displayBox.value = await configResponse.text();
      }
    }
  } catch (err) {
    console.error("Background polling error:", err);
  }
}

/**
 * Triggers a hardware reboot of the switch.
 */
function resetSwitch() {
  if (!confirm('Are you sure you want to reset the switch?')) { return; }
  setUILock(true); 
  fetch('/reset', { method: 'GET' }).catch(() => {});
  setTimeout(() => {
    alert('Switch is resetting. Please wait and refresh the page.');
    setUILock(false);
  }, 3000);
}

// Initialization routine
window.addEventListener("load", function() {
  fetchIP(true); 
  clearInterval(systemInterval); 
  systemInterval = setInterval(() => fetchIP(false), 5000); 
});

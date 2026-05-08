/**
 * system.js
 * * Handles frontend interactions for the RTLPlayground switch Web UI.
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
 * * @param {boolean} locked - True to lock UI, false to unlock.
 */
function setUILock(locked) {
  document.body.style.cursor = locked ? 'wait' : 'default';
  const buttons = document.querySelectorAll("button, input[type='button'], input[type='submit']");
  buttons.forEach(btn => btn.disabled = locked);
}

/**
 * Validates IPv4 address formatting strictly (0-255 octets).
 * * @param {string} ip - The IP string to test.
 * @returns {boolean} - True if valid, false if invalid.
 */
function checkIp(ip) {
  const ipv4 = /^(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)){3}$/;
  if (!ipv4.test(ip)) { alert(`Invalid IP format: ${ip}`); return false; }
  return true;
}

/**
 * Applies IP, Netmask, and Gateway sequentially. 
 * Designed as a strict transaction to prevent partial-state lockouts 
 * (e.g., applying an IP but failing to apply the matching subnet mask).
 */
async function ipSub() {
  // Pass 1: Validate all inputs before sending any commands to the hardware
  for (let i = 0; i < 3; i++) {
    if (!checkIp(document.getElementById(ips[i]).value)) return;
  }
  
  setUILock(true);
  
  try {
    // Pass 2: Send commands sequentially to the device RAM
    for (let i = 0; i < 3; i++) {
      const val = document.getElementById(ips[i]).value;
      const response = await fetch('/cmd', { method: 'POST', body: `${ips[i]} ${val}` });
      
      // Explicit ok check required because fetch() ignores 500-level server errors
      if (!response.ok) throw new Error(`HTTP Error: ${response.status}`);
      
      // Hardware throttle: Give the RTL8372 CPU 100ms to process before the next command
      await new Promise(r => setTimeout(r, 100)); 
    }
    // Await ensures the UI only updates after all commands are fully applied
    await fetchIP(false);
  } catch (err) {
    console.error(`Critical Error applying IP settings: ${err}`);
    alert(`Network update halted! Check console to prevent lockout.`);
    // Silent abort: prevents subsequent commands from applying over a broken state
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
 * Core Save Routine. Commits the merged configuration text blob directly to SPI Flash.
 * * @param {string} c - The raw configuration text block to save.
 */
async function sendConfig(c) {
  // Silent return if a save is already in progress
  if (isSaving) return; 
  
  isSaving = true;
  setUILock(true);
  
  // CRITICAL: Stop background IP polling. 
  // Writing to SPI flash halts the CPU. If a background poll hits the HTTP daemon 
  // during this halt, the web server crashes (ERR_EMPTY_RESPONSE).
  clearInterval(systemInterval);
  
  // Package config as a binary blob per executeconfig() C-backend expectations
  const form = new FormData();
  form.append("MAX_FILE_SIZE", "4096");
  form.append("configuration", new Blob([c], {type: "application/octet-stream"}));
  
  try {
    const response = await fetch('/config', { method: 'POST', body: form });
    if (!response.ok) throw new Error(`Server rejected flash write: ${response.status}`);
    
    console.log('Flash write completed successfully!');
    alert("Configuration successfully saved to flash memory.");
    
    // SAFEGUARD: Only clear the temporary RAM log if the flash write succeeded.
    // Otherwise, unsaved user input would be lost on a failed save.
    const clearResponse = await fetch('/cmd_log_clear', { method: 'GET' });
    if (!clearResponse.ok) throw new Error("Flash saved, but log clear failed.");
    console.log('Command log cleared.');
    
  } catch(err) {
    console.error(`Error writing to flash: ${err}`);
    alert("Save failed. Your configuration was NOT saved. Please check the console.");
  } finally {
    // ALWAYS release locks and restart polling, even if the save crashed
    isSaving = false;
    setUILock(false); 
    // Defensive reset: Clear interval again to prevent orphaned timer memory leaks
    clearInterval(systemInterval);
    systemInterval = setInterval(() => fetchIP(false), 5000);
  }
}

/**
 * Bypasses broken regex parsers by forwarding the exact UI textbox contents to the flash writer.
 */
async function flashSave() {
  flashStartupSave(); 
}

async function flashStartupSave() {
  const configContent = document.getElementById("config_display").value;
  await sendConfig(configContent);
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
 * * @param {boolean} forceConfigUpdate - If true, forcefully overwrites the config textbox. 
 * If false, only populates the box if it is currently empty.
 */
async function fetchIP(forceConfigUpdate = false) {
  // Never poll while the switch is busy writing to flash
  if (isSaving) return; 

  try {
    const response = await fetch('/information.json');
    if (!response.ok) throw new Error("Failed to fetch IP info");
    const s = await response.json();
    
    document.getElementById("ip").value = s.ip_address;
    document.getElementById("netmask").value = s.ip_netmask;
    document.getElementById("gw").value = s.ip_gateway;

    const displayBox = document.getElementById("config_display");
    
    // SAFEGUARD: Only update the heavy text box if forced (on load) or if empty.
    // This prevents background polls from deleting user typing mid-keystroke.
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
  setUILock(true); // Lock the UI to prevent interactions while the SoC restarts
  fetch('/reset', { method: 'GET' }).catch(() => {});
  setTimeout(() => {
    alert('Switch is resetting. Please wait and refresh the page.');
    setUILock(false);
  }, 3000);
}

// Initialization routine
window.addEventListener("load", function() {
  fetchIP(true); // Force populate the full config file into the UI on first boot
  clearInterval(systemInterval); // Defensively clear any phantom timers created by browser quirks
  systemInterval = setInterval(() => fetchIP(false), 5000); // Poll status every 5 seconds (not 1s)
});

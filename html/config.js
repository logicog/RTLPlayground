var configInterval = Number();
var configuration = [];

// THE WHITELIST (Sanitized)
const conf_cmds = [
  /^ip\s+(\d{1,3}\.){3}\d{1,3}/, 
  /^gw\s+(\d{1,3}\.){3}\d{1,3}/, 
  /^netmask\s+(\d{1,3}\.){3}\d{1,3}/,
  /^eee(?:\s+\d+)?\s+(on|off)/,   
  /^mirror/,                      
  /^vlan\s+\d+.*/,                
  /^pvid\s+\d+.*/,                
  /^lag\s+\d+.*/,                 
  /^bw\s+(in|out)\s+\d+.*/,       
  /^igmp\s+(on|off)/,             
  /^port\s+\d+.*/                 
];

// THE OVERWRITE KEYS (Sanitized)
const conf_overwrite = [
  /^ip/, 
  /^gw/, 
  /^netmask/, 
  /^eee(?:\s+\d+)?/,              
  /^mirror/,                      
  /^vlan\s+\d+/,                  
  /^pvid\s+\d+/,                  
  /^lag\s+\d+/,                   
  /^bw\s+(in|out)\s+\d+/,         
  /^igmp/,                        
  /^port\s+\d+/                   
];

function parseConf(s) {
  var a = s.split(/\r\n|\n/);
  for (var l = 0; l < a.length; l++) {
    
    // QUADRUPLE-CHECK FIX: Space Normalization.
    // 1. .trim() removes leading/trailing spaces and \r carriage returns.
    // 2. .replace(/\s+/g, ' ') turns any tabs or double-spaces into a single space.
    // This absolutely guarantees string-matching alignment.
    var line = a[l].trim().replace(/\s+/g, ' ');
    if (!line.length) continue;
    
    console.log(l + ' --> ' + line);
    var ignore = true;
    
    // Whitelist Check
    for (const x of conf_cmds) {
      if (x.test(line)) {
        ignore = false;
        break;
      }
    }
    
    if (ignore) continue;
    
    // Overwrite Logic
    for (const x of conf_overwrite) {
      if (x.test(line)) {
        let m = line.match(x);
        let matchStr = m[0]; 
        
        configuration = configuration.filter(item => {
          // Because we normalized all spaces above, we know for a fact that 
          // matchStr + " " will perfectly align with existing commands,
          // safely isolating 'pvid 1' from 'pvid 10' without double-space bugs.
          return !(item === matchStr || item.startsWith(matchStr + " "));
        });
        break; 
      }
    }
    configuration.push(line);
  }
}

async function fetchConfig() {
  try {
    const response = await fetch('/config');
    const t = await response.text();
    return t;
  } catch(err) {
    console.error("Error fetching config: ", err);
    return "";
  }
}

async function fetchCmdLog() {
  try {
    const response = await fetch('/cmd_log');
    const t = await response.text();
    return t;
  } catch(err) {
    console.error("Error fetching cmd_log: ", err);
    return "";
  }
}

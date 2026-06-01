var configInterval = Number();
var configuration = [];
const conf_cmds = [
  /^ip\s+(\d{1,3}\.){3}\d{1,3}$/,
  /^ip\s+dhcp$/,
  /^gw\s+(\d{1,3}\.){3}\d{1,3}$/,
  /^netmask\s+(\d{1,3}\.){3}\d{1,3}$/,
  /^syslog\s+(on|off)$/,
  /^syslog\s+ip\s+(\d{1,3}\.){3}\d{1,3}$/,
  /^passwd\s+\S+$/,
  /^vlan\s+\d{1,4}\s+d$/,
  /^vlan\s+\d{1,4}\s+mgmt$/,
  /^vlan\s+\d{1,4}(\s+[a-zA-Z]\w*)?(\s+\d{1,2}[tu]?)+$/,
  /^pvid\s+\d{1,2}\s+\d{1,4}$/,
  /^ingress(\s+\d{1,2}[tua])+$/,
  /^ingress\s+[tua]$/,
  /^port\s+\d{1,2}\s+name\s+\S+$/,
  /^eee(\s+\d{1,2})?\s+(on|off)$/,
  /^mirror(\s+\d{1,2})(\s+\d{1,2}[tr]?)+$/,
  /^lag\s+\d(\s+\d{1,2})+$/,
  /^laghash\s+\d(\s+\w+)+$/,
  /^isolate\s+\d{1,2}(\s+(off|\d{1,2}))+$/,
  /^stp\s+(on|off)$/,
  /^igmp\s+(on|off)$/,
  /^mtu\s+\d{1,2}\s+\d+$/,
  /^bw\s+(in|out)\s+\d{1,2}\s+\S+$/,
];
const conf_overwrite = [
  /^ip\b/,
  /^gw\b/,
  /^netmask\b/,
  /^syslog\s+ip\b/,
  /^syslog\b/,
  /^passwd\b/,
  /^vlan\s+\d{1,4}\s+mgmt$/,
  /^vlan\s+\d{1,4}(?!\s+mgmt\b)/,
  /^pvid\s+\d{1,2}\b/,
  /^ingress\b/,
  /^port\s+\d{1,2}\s+name\b/,
  /^eee\s+\d{1,2}\b/,
  /^eee\b/,
  /^mirror\b/,
  /^lag\s+\d+\b/,
  /^laghash\b/,
  /^isolate\s+\d{1,2}\b/,
  /^stp\b/,
  /^igmp\b/,
  /^mtu\s+\d{1,2}\b/,
  /^bw\s+(in|out)\s+\d{1,2}\b/,
];

function parseConf(s){
  var a = s.split(/\r\n|\n/);
  for (var l = 0; l < a.length; l++) {
    var line = a[l].trim().replace(/\s+/g, ' ');
    if (!line.length) continue;
    const deleteMatch = line.match(/^vlan\s+(\d{1,4})\s+d$/);
    if (deleteMatch) {
      const prefix = "vlan " + deleteMatch[1] + " ";
      configuration = configuration.filter(c =>
        c !== "vlan " + deleteMatch[1] && !c.startsWith(prefix));
      continue;
    }
    console.log(l + ' --> ' + line);
    var ignore = true;
    for (const x of conf_cmds)
      if (x.test(line)) { ignore = false; break; }
    if (ignore) continue;
    for (const x of conf_overwrite) {
      if (x.test(line)) {
        let m = line.match(x);
        let matchStr = m[0];
        configuration = configuration.filter(item =>
          !(item === matchStr || item.startsWith(matchStr + " ")));
        break;
      }
    }
    configuration.push(line);
  }
  console.log("Configuration now:");
  for (const x of configuration) { console.log(x); }
}

async function fetchConfig() {
  try {
    const response = await fetch('/config');
    console.log("CONFIG: ", response);
    const t = await response.text();
    return t;
  } catch(err) {
    console.error("Error: ", err);
  }
}

async function fetchCmdLog() {
  try {
    const response = await fetch('/cmd_log');
    console.log("CMD-Log: ", response);
    const t = await response.text();
    return t;
  } catch(err) {
    console.error("Error: ", err);
    return "";
  }
}

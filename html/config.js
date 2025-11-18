var configInterval = Number();
var configuration = [];
const conf_cmds = [
  /ip\s+(\d{1,3}\.){3}\d{1,3}/, /gw\s+(\d{1,3}\.){3}\d{1,3}/, /netmask\s+(\d{1,3}\.){3}\d{1,3}/,
  /eee(\s+\d)?\s+(on|off)/, /mirror(\s+(\d|10))(\s+(\d|10)(t|r)?)+/, /vlan\s+(\d{1,4})(\s+(\d|10)(t|u)?)+/
];
const conf_overwrite = [
  /ip/, /gw/, /netmask/, /eee\s+\w+/, /eee(\s+\w)/, /mirror/, /vlan\s+(\d{1,4})/
];

function parseConf(s){
  var a = s.split(/\r\n|\n/);
    for (var l = 0; l < a.length; l++) {
      if (!a[l].length || a[l] == "\n" || a[l] == "\r\n")
        continue;
      console.log(l + ' --> ' + a[l]);
      var ignore = true;
      for (const x of conf_cmds)
        if (x.test(a[l])) ignore = false;
      if (ignore) continue;
      for (const x of conf_overwrite) {
        if (x.test(a[l])) {
          console.log("Match ", x, " to ", a[l]);
          m = a[l].match(x);
          console.log("Starts with ", m[0]);
          configuration = configuration.filter(item => !(item.startsWith(m[0])));
        }
      }
      configuration.push(a[l]);
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
  } catch(error) {
    console.error("Error: ", err);
  }
}

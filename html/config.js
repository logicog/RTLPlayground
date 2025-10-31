var configInterval = Number();
var configuration = {};

function parseConf(c){
  var a = s.split(/\r\n|\n/);
    for (var l = 0; l < a.length; l++) {
      console.log(l + ' --> ' + a[l]);
    }
}

function fetchConfig() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      s = xhttp.responseText;
      console.log("CONFIG: ", s);
      parseConf(s);
      clearInterval(configInterval);
    }
  }
  xhttp.open("GET", `/config`, true);
  xhttp.send();
}

function fetchCmdLog() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      s = xhttp.responseText;
      console.log("CMD-Log: ", s);
      parseConf(s);
      clearInterval(configInterval);
    }
  }
  xhttp.open("GET", `/cmd_log`, true);
  xhttp.send();
}

window.addEventListener("load", function() {
  configInterval = setInterval(fetchConfig, 1000);
});

function update() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const s = JSON.parse(xhttp.responseText);
      console.log("Request returned", JSON.stringify(s));
      s.forEach(function (p, i) {
	let pid = "port" + p.portNum;
	console.log(p.txG, pid);
	var psvg = document.getElementById(pid);
	var bgs = psvg.contentDocument.getElementsByClassName("bg");
	console.log(JSON.stringify(bgs));
	var leds = psvg.contentDocument.getElementsByClassName("led");
	if (p.enabled == 0) {
	  bgs[0].style.fill = "red";
	  leds[0].style.fill = "black";
	  leds[1].style.fill = "black";
	  psvg.style.opacity = 0.4;
	} else {
	  psvg.style.opacity = 1.0;
	  if (p.link == 5) {
	    leds[0].style.fill = "green";
	    leds[1].style.fill = "orange";
	  } else if (p.link == 2) {
	    leds[0].style.fill = "green";
	    leds[1].style.fill = "green";
	  } else {
	    leds[0].style.fill = "black";
	    leds[1].style.fill = "black";
	    psvg.style.opacity = 0.4
	  }
	  console.log(JSON.stringify(leds));
	}
      });
    }
  };
  xhttp.open("GET", "/status.json", true);
  xhttp.send(); 
}

window.addEventListener("load", function() {
  update();
  const interval = setInterval(update, 5000);
});

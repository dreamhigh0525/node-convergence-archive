include("mjsunit.js");

var N = 40;
var finished = false;

function spawn (i) {
  var p = new node.Process('python -c "print 500 * 1024 * \'C\'"'); 
  var output = "";

  p.onOutput = function(chunk) { 
    if (chunk) output += chunk;
  }; 

  p.onExit = function () {
    //puts(output);
    if (i < N)
      spawn(i+1);
    else
      finished = true;
  };
}

function onLoad () {
  spawn(0);
}

function onExit () {
  assertTrue(finished);
}

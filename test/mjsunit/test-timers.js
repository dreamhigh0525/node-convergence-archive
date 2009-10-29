process.mixin(require("common.js"));

var WINDOW = 200; // why is does this need to be so big?

var interval_count = 0;
var setTimeout_called = false;

assertInstanceof(setTimeout, Function);
var starttime = new Date;

setTimeout(function () {
  var endtime = new Date;

  var diff = endtime - starttime;
  if (diff < 0) diff = -diff;
  puts("diff: " + diff);

  assertTrue(1000 - WINDOW < diff && diff < 1000 + WINDOW);
  setTimeout_called = true;
}, 1000);

// this timer shouldn't execute
var id = setTimeout(function () { assertTrue(false); }, 500);
clearTimeout(id);

setInterval(function () {
  interval_count += 1;
  var endtime = new Date;

  var diff = endtime - starttime;
  if (diff < 0) diff = -diff;
  puts("diff: " + diff);

  var t = interval_count * 1000;

  assertTrue(t - WINDOW < diff && diff < t + WINDOW);

  assertTrue(interval_count <= 3);
  if (interval_count == 3)
    clearInterval(this);
}, 1000);

process.addListener("exit", function () {
  assertTrue(setTimeout_called);
  assertEquals(3, interval_count);
});

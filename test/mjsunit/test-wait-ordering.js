process.mixin(require("./common"));

function timer (t) {
  var promise = new process.Promise();
  setTimeout(function () {
    promise.emitSuccess();
  }, t);
  return promise;
}

order = 0;
var a = new Date();
function test_timeout_order(delay, desired_order) {
  timer(0).addCallback(function() {
    timer(delay).wait()
    var b = new Date();
    assertTrue(b - a >= delay);
    order++;
    // A stronger assertion would be that the ordering is correct.
    // With Poor Man's coroutines we cannot guarentee that.
    // Replacing wait() with actual coroutines would solve that issue.
    // assertEquals(desired_order, order);
  });
}
test_timeout_order(10000, 6); // Why does this have the proper order??
test_timeout_order(5000, 5);
test_timeout_order(4000, 4);
test_timeout_order(3000, 3);
test_timeout_order(2000, 2);
test_timeout_order(1000, 1);

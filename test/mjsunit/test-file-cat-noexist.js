process.mixin(require("./common"));
var got_error = false;

var filename = path.join(fixturesDir, "does_not_exist.txt");
var promise = posix.cat(filename, "raw");

promise.addCallback(function (content) {
  debug("cat returned some content: " + content);
  debug("this shouldn't happen as the file doesn't exist...");
  assertTrue(false);
});

promise.addErrback(function () {
  got_error = true;
});

process.addListener("exit", function () {
  puts("done");
  assertTrue(got_error);
});

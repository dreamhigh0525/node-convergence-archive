libDir = node.path.join(node.path.dirname(__filename), "../lib");
node.libraryPaths.unshift(libDir);
include("/utils.js");
function next (i) {
  if (i <= 0) return;

  var child = node.createChildProcess("echo hello");

  child.addListener("output", function (chunk) {
    if (chunk) print(chunk);
  });

  child.addListener("exit", function (code) {
    if (code != 0) node.exit(-1);
    next(i - 1);
  });
}

next(500);

include("mjsunit.js");
PORT = 8888;

var body = "exports.A = function() { return 'A';}";
var server = node.http.createServer(function (req, res) {
  puts("got request");
  res.sendHeader(200, [
    ["Content-Length", body.length],
    ["Content-Type", "text/plain"]
  ]);
  res.sendBody(body);
  res.finish();
});
server.listen(PORT);

var got_good_server_content = false;
var bad_server_got_error = false;

node.http.cat("http://localhost:"+PORT+"/", "utf8").addCallback(function (content) {
  puts("got response");
  got_good_server_content = true;
  assertEquals(body, content);
  server.close();
});

node.http.cat("http://localhost:12312/", "utf8").addErrback(function () {
  puts("got error (this should happen)");
  bad_server_got_error = true;
});

process.addListener("exit", function () {
  assertTrue(got_good_server_content);
  assertTrue(bad_server_got_error);
});

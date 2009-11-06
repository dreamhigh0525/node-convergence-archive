process.mixin(require("./common"));
tcp = require("tcp");
http = require("http");

// Make sure no exceptions are thrown when receiving malformed HTTP
// requests.
port = 9999;

nrequests_completed = 0;
nrequests_expected = 1;

var s = http.createServer(function (req, res) {
  puts("req: " + JSON.stringify(req.uri));

  res.sendHeader(200, {"Content-Type": "text/plain"});
  res.sendBody("Hello World");
  res.finish();

  if (++nrequests_completed == nrequests_expected) s.close();
});
s.listen(port);

var c = tcp.createConnection(port);
c.addListener("connect", function () {
  c.send("GET /hello?foo=%99bar HTTP/1.1\r\n\r\n");
  c.close();
});

//  TODO add more!

process.addListener("exit", function () {
  assertEquals(nrequests_expected, nrequests_completed);
});

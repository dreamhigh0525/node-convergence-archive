include("common.js");
PORT = 8888;

var body1_s = "1111111111111111";
var body2_s = "22222";

var server = node.http.createServer(function (req, res) {
  var body = req.uri.path === "/1" ? body1_s : body2_s;
  res.sendHeader(200, { "Content-Type": "text/plain"
                      , "Content-Length": body.length
                      });
  res.sendBody(body);
  res.finish();
});
server.listen(PORT);

var client = node.http.createClient(PORT);

var body1 = "";
var body2 = "";

client.get("/1").finish(function (res1) {
  res1.setBodyEncoding("utf8");

  res1.addListener("body", function (chunk) {
    body1 += chunk;
  });

  res1.addListener("complete", function () {
    client.get("/2").finish(function (res2) {
      res2.setBodyEncoding("utf8");
      res2.addListener("body", function (chunk) { body2 += chunk; });
      res2.addListener("complete", function () { server.close(); });
    });
  });
});

process.addListener("exit", function () {
  assertEquals(body1_s, body1);
  assertEquals(body2_s, body2);
});

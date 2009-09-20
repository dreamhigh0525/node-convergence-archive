include("common.js");
var PORT = 18032;

var sent_body = "";
var server_req_complete = false;
var client_res_complete = false;

var server = node.http.createServer(function(req, res) {
  assertEquals("POST", req.method);
  req.setBodyEncoding("utf8");

  req.addListener("body", function (chunk) {
    puts("server got: " + JSON.stringify(chunk));
    sent_body += chunk;
  });

  req.addListener("complete", function () {
    server_req_complete = true;
    puts("request complete from server");
    res.sendHeader(200, {'Content-Type': 'text/plain'});
    res.sendBody('hello\n');
    res.finish();
  });
});
server.listen(PORT);

var client = node.http.createClient(PORT);
var req = client.post('/');

req.sendBody('1\n');
req.sendBody('2\n');
req.sendBody('3\n');

puts("client finished sending request");
req.finish(function(res) {
  res.setBodyEncoding("utf8");
  res.addListener('body', function(chunk) {
    puts(chunk);
  });
  res.addListener('complete', function() {
    client_res_complete = true;
    server.close();
  });
});

process.addListener("exit", function () {
  assertEquals("1\n2\n3\n", sent_body);
  assertTrue(server_req_complete);
  assertTrue(client_res_complete);
});

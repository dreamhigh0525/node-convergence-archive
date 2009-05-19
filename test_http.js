new node.http.Server(function (req, res) {
  puts("got req to " + req.uri.path);
  setTimeout(function () {
    res.sendHeader(200, [["Content-Type", "text/plain"]]);
    res.sendBody(JSON.stringify(req.uri));
    res.finish();
  }, 1);
}).listen(8000, "localhost");

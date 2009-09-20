include("common.js");
var N = 50;
var port = 8921;

var c = 0;
var client_recv_count = 0;
var disconnect_count = 0;

var server = node.tcp.createServer(function (socket) {
  socket.addListener("connect", function () {
    socket.send("hello\r\n");
  });

  socket.addListener("eof", function () {
    socket.close();
  });

  socket.addListener("close", function (had_error) {
    //puts("server had_error: " + JSON.stringify(had_error));
    assertFalse(had_error);
  });
});
server.listen(port);

var client = node.tcp.createConnection(port);

client.setEncoding("UTF8");

client.addListener("connect", function () {
  puts("client connected.");
});

client.addListener("receive", function (chunk) {
  client_recv_count += 1;
  puts("client_recv_count " + client_recv_count);
  assertEquals("hello\r\n", chunk);
  client.close();
});

client.addListener("close", function (had_error) {
  puts("disconnect");
  assertFalse(had_error);
  if (disconnect_count++ < N)
    client.connect(port); // reconnect
  else
    server.close();
});

process.addListener("exit", function () {
  assertEquals(N+1, disconnect_count);
  assertEquals(N+1, client_recv_count);
});

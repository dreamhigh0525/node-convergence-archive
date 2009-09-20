include("common.js");
port = 9992;
exchanges = 0;
starttime = null;
timeouttime = null;
timeout = 1000;

var echo_server = node.tcp.createServer(function (socket) {
  socket.setTimeout(timeout);

  socket.addListener("timeout", function (d) {
    puts("server timeout");
    timeouttime = new Date;
    p(timeouttime);
  });

  socket.addListener("receive", function (d) {
    p(d);
    socket.send(d);
  });

  socket.addListener("eof", function () {
    socket.close();
  });
});

echo_server.listen(port);
puts("server listening at " + port);

var client = node.tcp.createConnection(port);
client.setEncoding("UTF8");
client.setTimeout(0); // disable the timeout for client
client.addListener("connect", function () {
  puts("client connected.");
  client.send("hello\r\n");
});

client.addListener("receive", function (chunk) {
  assertEquals("hello\r\n", chunk);
  if (exchanges++ < 5) {
    setTimeout(function () {
      puts("client send 'hello'");
      client.send("hello\r\n");
    }, 500);

    if (exchanges == 5) {
      puts("wait for timeout - should come in " + timeout + " ms");
      starttime = new Date;
      p(starttime);
    }
  }
});

client.addListener("timeout", function () {
  puts("client timeout - this shouldn't happen");
  assertFalse(true);
});

client.addListener("eof", function () {
  puts("client eof");
  client.close();
});

client.addListener("close", function (had_error) {
  puts("client disconnect");
  echo_server.close();
  assertFalse(had_error);
});

process.addListener("exit", function () {
  assertTrue(starttime != null);
  assertTrue(timeouttime != null);

  diff = timeouttime - starttime;
  puts("diff = " + diff);
  assertTrue(timeout < diff);
  // Allow for 800 milliseconds more
  assertTrue(diff < timeout + 800);
});

include("mjsunit.js");


var tests_run = 0;

function pingPongTest (port, host, on_complete) {
  var N = 100;
  var DELAY = 1;
  var count = 0;
  var client_closed = false;

  var server = node.tcp.createServer(function (socket) {
    socket.setEncoding("utf8");

    socket.addListener("receive", function (data) {
      puts(data);
      assertEquals("PING", data);
      assertEquals("open", socket.readyState);
      assertTrue(count <= N);
      setTimeout(function () {
        assertEquals("open", socket.readyState);
        socket.send("PONG");
      }, DELAY);
    });

    socket.addListener("timeout", function () {
      node.debug("server-side timeout!!");
      assertFalse(true);
    });

    socket.addListener("eof", function () {
      puts("server-side socket EOF");
      assertEquals("writeOnly", socket.readyState);
      socket.close();
    });

    socket.addListener("disconnect", function (had_error) {
      puts("server-side socket disconnect");
      assertFalse(had_error);
      assertEquals("closed", socket.readyState);
      socket.server.close();
    });
  });
  server.listen(port, host);

  var client = node.tcp.createConnection(port, host);

  client.setEncoding("utf8");

  client.addListener("connect", function () {
    assertEquals("open", client.readyState);
    client.send("PING");
  });

  client.addListener("receive", function (data) {
    puts(data);
    assertEquals("PONG", data);
    assertEquals("open", client.readyState);

    setTimeout(function () {
      assertEquals("open", client.readyState);
      if (count++ < N) {
        client.send("PING");
      } else {
        puts("closing client");
        client.close();
        client_closed = true;
      }
    }, DELAY);
  });

  client.addListener("timeout", function () {
    node.debug("client-side timeout!!");
    assertFalse(true);
  });

  client.addListener("disconnect", function () {
    puts("client disconnect");
    assertEquals(N+1, count);
    assertTrue(client_closed);
    if (on_complete) on_complete();
    tests_run += 1;
  });
}

function onLoad () {
  pingPongTest(21988);
}

function onExit () {
  assertEquals(1, tests_run);
}

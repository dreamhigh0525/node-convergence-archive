exports.createServer = function (on_connection, options) {
  var server = new node.tcp.Server();
  server.addListener("connection", on_connection);
  //server.setOptions(options);
  return server;
};

exports.createConnection = function (port, host) {
  var connection = new node.tcp.Connection();
  connection.connect(port, host);
  return connection;
};

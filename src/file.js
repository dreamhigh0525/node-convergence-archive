node.fs.exists = function (path, callback) {
  var p = node.fs.stat(path);
  p.addCallback(function () { callback(true); });
  p.addErrback(function () { callback(false); });
};

node.fs.cat = function (path, encoding) {
  var promise = new node.Promise();
  
  encoding = encoding || "utf8"; // default to utf8

  node.fs.open(path, node.O_RDONLY, 0666).addCallback(function (fd) {
    var content = "", pos = 0;

    function readChunk () {
      node.fs.read(fd, 16*1024, pos, encoding).addCallback(function (chunk, bytes_read) {
        if (chunk) {
          if (chunk.constructor === String) {
            content += chunk;
          } else {
            content = content.concat(chunk);
          }

          pos += bytes_read;
          readChunk();
        } else {
          promise.emitSuccess(content);
          node.fs.close(fd);
        }
      }).addErrback(function () {
        promise.emitError();
      });
    }
    readChunk();
  }).addErrback(function () {
    promise.emitError(new Error("Could not open " + path));
  });
  return promise;
};

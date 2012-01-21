var fs = require('fs');
var http = require('http');
var isolates = process.binding('isolates');

console.log("count: %d", isolates.count());

if (process.tid === 1) {
  var isolate = isolates.create(process.argv, {
    debug: function init(d) {
      d.onmessage = function(data) {
        data = JSON.parse(data);
        if (data.event === 'break') {
          d.write(JSON.stringify({
            type: 'request',
            seq: 1,
            command: 'continue'
          }));
        }
      };
    }
  });

  isolate.onmessage = function() {
    console.error("onmessage");
  };
  isolate.onexit = function() {
    console.error("onexit");
  };

  console.error("master");
  fs.stat(__dirname, function(err, stat) {
    if (err) throw err;
    console.error('thread 1', stat.mtime);
  });

  setTimeout(function() {
    fs.stat(__dirname, function(err, stat) {
      if (err) throw err;
      console.error('thread 1', stat.mtime);
    });
  }, 500);

  console.log("thread 1 count: %d", isolates.count());
} else {
  console.error("slave");
  fs.stat(__dirname, function(err, stat) {
    if (err) throw err;
    console.error('thread 2', stat.mtime);
  });

  setTimeout(function() {
    fs.stat(__dirname, function(err, stat) {
      if (err) throw err;
      console.error('thread 2', stat.mtime);
      process.exit();
    });
  }, 500);

  console.error("thread 2 count: %d", isolates.count());
}

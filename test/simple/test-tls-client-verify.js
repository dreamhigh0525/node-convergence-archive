var testCases =
  [ { ca: ['ca1-cert'],
      key: 'agent2-key',
      cert: 'agent2-cert',
      servers: [
        { ok: true, key: 'agent1-key', cert: 'agent1-cert' },
        { ok: false, key: 'agent2-key', cert: 'agent2-cert' },
        { ok: false, key: 'agent3-key', cert: 'agent3-cert' },
      ]
    }, 
  
    { ca: [],
      key: 'agent2-key',
      cert: 'agent2-cert',
      servers: [
        { ok: false, key: 'agent1-key', cert: 'agent1-cert' },
        { ok: false, key: 'agent2-key', cert: 'agent2-cert' },
        { ok: false, key: 'agent3-key', cert: 'agent3-cert' },
      ]
    },

    { ca: ['ca1-cert', 'ca2-cert'],
      key: 'agent2-key',
      cert: 'agent2-cert',
      servers: [
        { ok: true, key: 'agent1-key', cert: 'agent1-cert' },
        { ok: false, key: 'agent2-key', cert: 'agent2-cert' },
        { ok: true, key: 'agent3-key', cert: 'agent3-cert' },
      ]
    },
  ];


var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var tls = require('tls');


function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + ".pem");
}


function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

var successfulTests = 0;

function testServers(index, servers, clientOptions, cb) {
  var serverOptions = servers[index];
  if (!serverOptions) {
    cb();
    return;
  }

  var ok = serverOptions.ok;

  if (serverOptions.key) {
    serverOptions.key = loadPEM(serverOptions.key); 
  }

  if (serverOptions.cert) { 
    serverOptions.cert = loadPEM(serverOptions.cert); 
  }

  var server = tls.createServer(serverOptions, function(s) {
    s.end("hello world\n"); 
  });

  server.listen(common.PORT, function() {
    var b = '';

    console.error("connecting...");
    var client = tls.connect(common.PORT, clientOptions, function () {

      console.error("expected: " + ok + " authed: " + client.authorized);

      assert.equal(ok, client.authorized); 
      server.close();
    });

    client.on('data', function(d) {
      b += d.toString();
    });

    client.on('end', function() {
      // TODO: 
      //assert.equal('hello world\n', b);
    });

    client.on('close', function() {
      testServers(index + 1, servers, clientOptions, cb);
    });
  });
}


function runTest (testIndex) {
  var tcase = testCases[testIndex];
  if (!tcase) return;

  var clientOptions = {
    ca: tcase.ca.map(loadPEM),
    key: loadPEM(tcase.key),
    cert: loadPEM(tcase.cert)
  };


  testServers(0, tcase.servers, clientOptions, function () {
    successfulTests++;
    runTest(testIndex + 1);
  });
}


runTest(0);


process.on('exit', function() {
  console.log("successful tests: %d", successfulTests);
  assert.equal(successfulTests, testCases.length);
});

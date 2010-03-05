process.mixin(require('../common'));

var
  fn = path.join(fixturesDir, 'multipart.js'),
  file = fs.fileReadStream(fn),

  callbacks = {
    open: -1,
    end: -1,
    close: -1
  },

  paused = false,

  fileContent = '';

file
  .addListener('open', function(fd) {
    callbacks.open++;
    assert.equal('number', typeof fd);
    assert.ok(file.readable);
  })
  .addListener('error', function(err) {
    throw err;
  })
  .addListener('data', function(data) {
    assert.ok(!paused);
    fileContent += data;
    
    paused = true;
    file.pause();
    assert.ok(file.paused);

    setTimeout(function() {
      paused = false;
      file.resume();
      assert.ok(!file.paused);
    }, 10);
  })
  .addListener('end', function(chunk) {
    callbacks.end++;
  })
  .addListener('close', function() {
    callbacks.close++;
    assert.ok(!file.readable);

    assert.equal(fs.readFileSync(fn), fileContent);
  });

process.addListener('exit', function() {
  for (var k in callbacks) {
    assert.equal(0, callbacks[k], k+' count off by '+callbacks[k]);
  }
});
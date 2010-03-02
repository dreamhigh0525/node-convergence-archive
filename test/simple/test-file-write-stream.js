process.mixin(require('../common'));

var
  fn = path.join(fixturesDir, "write.txt"),
  file = fs.fileWriteStream(fn),

  EXPECTED = '0123456789',

  callbacks = {
    open: -1,
    drain: -2,
    close: -1
  };

file
  .addListener('open', function(fd) {
    callbacks.open++;
    assert.equal('number', typeof fd);
  })
  .addListener('drain', function() {
    callbacks.drain++;
    if (callbacks.drain == -1) {
      assert.equal(EXPECTED, fs.readFileSync(fn));
      file.write(EXPECTED);
    } else if (callbacks.drain == 0) {
      assert.equal(EXPECTED+EXPECTED, fs.readFileSync(fn));
      file.close();
    }
  })
  .addListener('close', function() {
    callbacks.close++;
    assert.throws(function() {
      file.write('should not work anymore');
    });

    fs.unlinkSync(fn);
  });

for (var i = 0; i < 10; i++) {
  assert.strictEqual(false, file.write(i));
}

process.addListener('exit', function() {
  for (var k in callbacks) {
    assert.equal(0, callbacks[k], k+' count off by '+callbacks[k]);
  }
});
/*
 * Tests to verify we're reading in floats correctly
 */
var ASSERT = require('assert');

/*
 * Test (32 bit) float
 */
function test() {
  var buffer = new Buffer(4);
  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0x3f;
  ASSERT.equal(4.600602988224807e-41, buffer.readFloat(0, 'big'));
  ASSERT.equal(1, buffer.readFloat(0, 'little'));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0xc0;
  ASSERT.equal(2.6904930515036488e-43, buffer.readFloat(0, 'big'));
  ASSERT.equal(-2, buffer.readFloat(0, 'little'));

  buffer[0] = 0xff;
  buffer[1] = 0xff;
  buffer[2] = 0x7f;
  buffer[3] = 0x7f;
  ASSERT.ok(isNaN(buffer.readFloat(0, 'big')));
  ASSERT.equal(3.4028234663852886e+38, buffer.readFloat(0, 'little'));

  buffer[0] = 0xab;
  buffer[1] = 0xaa;
  buffer[2] = 0xaa;
  buffer[3] = 0x3e;
  ASSERT.equal(-1.2126478207002966e-12, buffer.readFloat(0, 'big'));
  ASSERT.equal(0.3333333432674408, buffer.readFloat(0, 'little'));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0;
  ASSERT.equal(0, buffer.readFloat(0, 'big'));
  ASSERT.equal(0, buffer.readFloat(0, 'little'));
  ASSERT.equal(false, 1/buffer.readFloat(0, 'little')<0);

  buffer[3] = 0x80;
  ASSERT.equal(1.793662034335766e-43, buffer.readFloat(0, 'big'));
  ASSERT.equal(0, buffer.readFloat(0, 'little'));
  ASSERT.equal(true, 1/buffer.readFloat(0, 'little')<0);

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0x7f;
  ASSERT.equal(4.609571298396486e-41, buffer.readFloat(0, 'big'));
  ASSERT.equal(Infinity, buffer.readFloat(0, 'little'));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0xff;
  ASSERT.equal(4.627507918739843e-41, buffer.readFloat(0, 'big'));
  ASSERT.equal(-Infinity, buffer.readFloat(0, 'little'));
}


test();

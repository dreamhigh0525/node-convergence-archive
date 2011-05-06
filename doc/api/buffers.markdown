## Buffers

Pure Javascript is Unicode friendly but not nice to binary data.  When
dealing with TCP streams or the file system, it's necessary to handle octet
streams. Node has several strategies for manipulating, creating, and
consuming octet streams.

Raw data is stored in instances of the `Buffer` class. A `Buffer` is similar
to an array of integers but corresponds to a raw memory allocation outside
the V8 heap. A `Buffer` cannot be resized.

The `Buffer` object is global.

Converting between Buffers and JavaScript string objects requires an explicit encoding
method.  Here are the different string encodings;

* `'ascii'` - for 7 bit ASCII data only.  This encoding method is very fast, and will
strip the high bit if set.

* `'utf8'` - Multi byte encoded Unicode characters.  Many web pages and other document formats use UTF-8.

* `'ucs2'` - 2-bytes, little endian encoded Unicode characters. It can encode
only BMP(Basic Multilingual Plane, U+0000 - U+FFFF).

* `'base64'` - Base64 string encoding.

* `'binary'` - A way of encoding raw binary data into strings by using only
the first 8 bits of each character. This encoding method is deprecated and
should be avoided in favor of `Buffer` objects where possible. This encoding
will be removed in future versions of Node.

* `'hex'` - Encode each byte as two hexidecimal characters.


### new Buffer(size)

Allocates a new buffer of `size` octets.

### new Buffer(array)

Allocates a new buffer using an `array` of octets.

### new Buffer(str, encoding='utf8')

Allocates a new buffer containing the given `str`.

### buffer.write(string, offset=0, encoding='utf8')

Writes `string` to the buffer at `offset` using the given encoding. Returns
number of octets written.  If `buffer` did not contain enough space to fit
the entire string, it will write a partial amount of the string.
The method will not write partial characters.

Example: write a utf8 string into a buffer, then print it

    buf = new Buffer(256);
    len = buf.write('\u00bd + \u00bc = \u00be', 0);
    console.log(len + " bytes: " + buf.toString('utf8', 0, len));

The number of characters written (which may be different than the number of
bytes written) is set in `Buffer._charsWritten` and will be overwritten the
next time `buf.write()` is called.


### buffer.toString(encoding, start=0, end=buffer.length)

Decodes and returns a string from buffer data encoded with `encoding`
beginning at `start` and ending at `end`.

See `buffer.write()` example, above.


### buffer[index]

Get and set the octet at `index`. The values refer to individual bytes,
so the legal range is between `0x00` and `0xFF` hex or `0` and `255`.

Example: copy an ASCII string into a buffer, one byte at a time:

    str = "node.js";
    buf = new Buffer(str.length);

    for (var i = 0; i < str.length ; i++) {
      buf[i] = str.charCodeAt(i);
    }

    console.log(buf);

    // node.js

### Buffer.isBuffer(obj)

Tests if `obj` is a `Buffer`.

### Buffer.byteLength(string, encoding='utf8')

Gives the actual byte length of a string.  This is not the same as
`String.prototype.length` since that returns the number of *characters* in a
string.

Example:

    str = '\u00bd + \u00bc = \u00be';

    console.log(str + ": " + str.length + " characters, " +
      Buffer.byteLength(str, 'utf8') + " bytes");

    // ½ + ¼ = ¾: 9 characters, 12 bytes


### buffer.length

The size of the buffer in bytes.  Note that this is not necessarily the size
of the contents. `length` refers to the amount of memory allocated for the
buffer object.  It does not change when the contents of the buffer are changed.

    buf = new Buffer(1234);

    console.log(buf.length);
    buf.write("some string", "ascii", 0);
    console.log(buf.length);

    // 1234
    // 1234

### buffer.copy(targetBuffer, targetStart=0, sourceStart=0, sourceEnd=buffer.length)

Does a memcpy() between buffers.

Example: build two Buffers, then copy `buf1` from byte 16 through byte 19
into `buf2`, starting at the 8th byte in `buf2`.

    buf1 = new Buffer(26);
    buf2 = new Buffer(26);

    for (var i = 0 ; i < 26 ; i++) {
      buf1[i] = i + 97; // 97 is ASCII a
      buf2[i] = 33; // ASCII !
    }

    buf1.copy(buf2, 8, 16, 20);
    console.log(buf2.toString('ascii', 0, 25));

    // !!!!!!!!qrst!!!!!!!!!!!!!


### buffer.slice(start, end=buffer.length)

Returns a new buffer which references the
same memory as the old, but offset and cropped by the `start` and `end`
indexes.

**Modifying the new buffer slice will modify memory in the original buffer!**

Example: build a Buffer with the ASCII alphabet, take a slice, then modify one byte
from the original Buffer.

    var buf1 = new Buffer(26);

    for (var i = 0 ; i < 26 ; i++) {
      buf1[i] = i + 97; // 97 is ASCII a
    }

    var buf2 = buf1.slice(0, 3);
    console.log(buf2.toString('ascii', 0, buf2.length));
    buf1[0] = 33;
    console.log(buf2.toString('ascii', 0, buf2.length));

    // abc
    // !bc

### buffer.readUInt8(offset, endian)

Reads an unsigned 8 bit integer from the buffer at the specified offset. Endian
must be either 'big' or 'little' and specifies what endian ordering to read the
bytes from the buffer in.

Example:

    var buf = new Buffer(4);

    buf[0] = 0x3;
    buf[1] = 0x4;
    buf[2] = 0x23;
    buf[3] = 0x42;

    for (ii = 0; ii < buf.length; ii++) {
      console.log(buf.readUInt8(ii, 'big');
      console.log(buf.readUInt8(ii, 'little');
    }

    // 0x3
    // 0x3
    // 0x4
    // 0x4
    // 0x23
    // 0x23
    // 0x42
    // 0x42

### buffer.readUInt16(offset, endian)

Reads an unsigned 16 bit integer from the buffer at the specified offset. Endian
must be either 'big' or 'little' and specifies what endian ordering to read the
bytes from the buffer in.

Example:

    var buf = new Buffer(4);

    buf[0] = 0x3;
    buf[1] = 0x4;
    buf[2] = 0x23;
    buf[3] = 0x42;

    console.log(buf.readUInt16(0, 'big');
    console.log(buf.readUInt16(0, 'little');
    console.log(buf.readUInt16(1, 'big');
    console.log(buf.readUInt16(1, 'little');
    console.log(buf.readUInt16(2, 'big');
    console.log(buf.readUInt16(2, 'little');

    // 0x0304
    // 0x0403
    // 0x0423
    // 0x2304
    // 0x2342
    // 0x4223

### buffer.readUInt32(offset, endian)

Reads an unsigned 32 bit integer from the buffer at the specified offset. Endian
must be either 'big' or 'little' and specifies what endian ordering to read the
bytes from the buffer in.

Example:

    var buf = new Buffer(4);

    buf[0] = 0x3;
    buf[1] = 0x4;
    buf[2] = 0x23;
    buf[3] = 0x42;

    console.log(buf.readUInt32(0, 'big');
    console.log(buf.readUInt32(0, 'little');

    // 0x03042342
    // 0x42230403

### buffer.readInt8(offset, endian)

Reads a signed 8 bit integer from the buffer at the specified offset. Endian
must be either 'big' or 'little' and specifies what endian ordering to read the
bytes from the buffer in.

Works as `buffer.readUInt8`, except buffer contents are treated as twos
complement signed values.

### buffer.readInt16(offset, endian)

Reads a signed 16 bit integer from the buffer at the specified offset. Endian
must be either 'big' or 'little' and specifies what endian ordering to read the
bytes from the buffer in.

Works as `buffer.readUInt16`, except buffer contents are treated as twos
complement signed values.

### buffer.readInt32(offset, endian)

Reads a signed 32 bit integer from the buffer at the specified offset. Endian
must be either 'big' or 'little' and specifies what endian ordering to read the
bytes from the buffer in.

Works as `buffer.readUInt32`, except buffer contents are treated as twos
complement signed values.

### buffer.writeUInt8(value, offset, endian)

Writes `value` to the buffer at the specified offset with specified endian
format. Note, `value` must be a valid 8 bit unsigned integer.

Example:

    var buf = new Buffer(4);
    buf.writeUInt8(0x3, 0, 'big');
    buf.writeUInt8(0x4, 1, 'big');
    buf.writeUInt8(0x23, 2, 'big');
    buf.writeUInt8(0x42, 3, 'big');

    console.log(buf);

    buf.writeUInt8(0x3, 0, 'little');
    buf.writeUInt8(0x4, 1, 'little');
    buf.writeUInt8(0x23, 2, 'little');
    buf.writeUInt8(0x42, 3, 'little');

    console.log(buf);

    // <Buffer 03 04 23 42>
    // <Buffer 03 04 23 42>

### buffer.writeUInt16(value, offset, endian)

Writes `value` to the buffer at the specified offset with specified endian
format. Note, `value` must be a valid 16 bit unsigned integer.

Example:

    var buf = new Buffer(4);
    buf.writeUInt16(0xdead, 0, 'big');
    buf.writeUInt16(0xbeef, 2, 'big');

    console.log(buf);

    buf.writeUInt16(0xdead, 0, 'little');
    buf.writeUInt16(0xbeef, 2, 'little');

    console.log(buf);

    // <Buffer de ad be ef>
    // <Buffer ad de ef be>

### buffer.writeUInt32(value, offset, endian)

Writes `value` to the buffer at the specified offset with specified endian
format. Note, `value` must be a valid 32 bit unsigned integer.

Example:

    var buf = new Buffer(4);
    buf.writeUInt32(0xfeedface, 0, 'big');

    console.log(buf);

    buf.writeUInt32(0xfeedface, 0, 'little');

    console.log(buf);

    // <Buffer fe ed fa ce>
    // <Buffer ce fa ed fe>

### buffer.writeInt8(value, offset, endian)

Writes `value` to the buffer at the specified offset with specified endian
format. Note, `value` must be a valid 16 bit signed integer.

Works as `buffer.writeUInt8`, except value is written out as a two's complement
signed integer into `buffer`.

### buffer.writeInt16(value, offset, endian)

Writes `value` to the buffer at the specified offset with specified endian
format. Note, `value` must be a valid 16 bit unsigned integer.

Works as `buffer.writeUInt16`, except value is written out as a two's complement
signed integer into `buffer`.

### buffer.writeInt32(value, offset, endian)

Writes `value` to the buffer at the specified offset with specified endian
format. Note, `value` must be a valid 16 bit signed integer.

Works as `buffer.writeUInt832, except value is written out as a two's complement
signed integer into `buffer`.


### buffer.fill(value, offset=0, length=-1)

Fills the buffer with the specified value. If the offset and length are not
given it will fill the entire buffer.

    var b = new Buffer(50);
    b.fill("h");


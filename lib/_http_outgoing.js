// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var assert = require('assert').ok;
var Stream = require('stream');
var timers = require('timers');
var util = require('util');

var common = require('_http_common');

var CRLF = common.CRLF;
var chunkExpression = common.chunkExpression;
var continueExpression = common.continueExpression;
var debug = common.debug;


var connectionExpression = /Connection/i;
var transferEncodingExpression = /Transfer-Encoding/i;
var closeExpression = /close/i;
var contentLengthExpression = /Content-Length/i;
var dateExpression = /Date/i;
var expectExpression = /Expect/i;


var dateCache;
function utcDate() {
  if (!dateCache) {
    var d = new Date();
    dateCache = d.toUTCString();
    timers.enroll(utcDate, 1000 - d.getMilliseconds());
    timers._unrefActive(utcDate);
  }
  return dateCache;
}
utcDate._onTimeout = function() {
  dateCache = undefined;
};


function OutgoingMessage() {
  Stream.call(this);

  this.output = [];
  this.outputEncodings = [];

  this.writable = true;

  this._last = false;
  this.chunkedEncoding = false;
  this.shouldKeepAlive = true;
  this.useChunkedEncodingByDefault = true;
  this.sendDate = false;

  this._hasBody = true;
  this._trailer = '';

  this.finished = false;
  this._hangupClose = false;

  this.socket = null;
  this.connection = null;
}
util.inherits(OutgoingMessage, Stream);


exports.OutgoingMessage = OutgoingMessage;


OutgoingMessage.prototype.setTimeout = function(msecs, callback) {
  if (callback)
    this.on('timeout', callback);
  if (!this.socket) {
    this.once('socket', function(socket) {
      socket.setTimeout(msecs);
    });
  } else
    this.socket.setTimeout(msecs);
};


// It's possible that the socket will be destroyed, and removed from
// any messages, before ever calling this.  In that case, just skip
// it, since something else is destroying this connection anyway.
OutgoingMessage.prototype.destroy = function(error) {
  if (this.socket)
    this.socket.destroy(error);
  else
    this.once('socket', function(socket) {
      socket.destroy(error);
    });
};


// This abstract either writing directly to the socket or buffering it.
OutgoingMessage.prototype._send = function(data, encoding) {
  // This is a shameful hack to get the headers and first body chunk onto
  // the same packet. Future versions of Node are going to take care of
  // this at a lower level and in a more general way.
  if (!this._headerSent) {
    if (typeof data === 'string') {
      data = this._header + data;
    } else {
      this.output.unshift(this._header);
      this.outputEncodings.unshift('ascii');
    }
    this._headerSent = true;
  }
  return this._writeRaw(data, encoding);
};


OutgoingMessage.prototype._writeRaw = function(data, encoding) {
  if (data.length === 0) {
    return true;
  }

  if (this.connection &&
      this.connection._httpMessage === this &&
      this.connection.writable &&
      !this.connection.destroyed) {
    // There might be pending data in the this.output buffer.
    while (this.output.length) {
      if (!this.connection.writable) {
        this._buffer(data, encoding);
        return false;
      }
      var c = this.output.shift();
      var e = this.outputEncodings.shift();
      this.connection.write(c, e);
    }

    // Directly write to socket.
    return this.connection.write(data, encoding);
  } else if (this.connection && this.connection.destroyed) {
    // The socket was destroyed.  If we're still trying to write to it,
    // then we haven't gotten the 'close' event yet.
    return false;
  } else {
    // buffer, as long as we're not destroyed.
    this._buffer(data, encoding);
    return false;
  }
};


OutgoingMessage.prototype._buffer = function(data, encoding) {
  if (data.length === 0) return;

  var length = this.output.length;

  if (length === 0 || typeof data != 'string') {
    this.output.push(data);
    this.outputEncodings.push(encoding);
    return false;
  }

  var lastEncoding = this.outputEncodings[length - 1];
  var lastData = this.output[length - 1];

  if ((encoding && lastEncoding === encoding) ||
      (!encoding && data.constructor === lastData.constructor)) {
    this.output[length - 1] = lastData + data;
    return false;
  }

  this.output.push(data);
  this.outputEncodings.push(encoding);

  return false;
};


OutgoingMessage.prototype._storeHeader = function(firstLine, headers) {
  // firstLine in the case of request is: 'GET /index.html HTTP/1.1\r\n'
  // in the case of response it is: 'HTTP/1.1 200 OK\r\n'
  var state = {
    sentConnectionHeader: false,
    sentContentLengthHeader: false,
    sentTransferEncodingHeader: false,
    sentDateHeader: false,
    sentExpect: false,
    messageHeader: firstLine
  };

  var field, value;
  var self = this;

  if (headers) {
    var keys = Object.keys(headers);
    var isArray = (Array.isArray(headers));
    var field, value;

    for (var i = 0, l = keys.length; i < l; i++) {
      var key = keys[i];
      if (isArray) {
        field = headers[key][0];
        value = headers[key][1];
      } else {
        field = key;
        value = headers[key];
      }

      if (Array.isArray(value)) {
        for (var j = 0; j < value.length; j++) {
          storeHeader(this, state, field, value[j]);
        }
      } else {
        storeHeader(this, state, field, value);
      }
    }
  }

  // Date header
  if (this.sendDate == true && state.sentDateHeader == false) {
    state.messageHeader += 'Date: ' + utcDate() + CRLF;
  }

  // Force the connection to close when the response is a 204 No Content or
  // a 304 Not Modified and the user has set a "Transfer-Encoding: chunked"
  // header.
  //
  // RFC 2616 mandates that 204 and 304 responses MUST NOT have a body but
  // node.js used to send out a zero chunk anyway to accommodate clients
  // that don't have special handling for those responses.
  //
  // It was pointed out that this might confuse reverse proxies to the point
  // of creating security liabilities, so suppress the zero chunk and force
  // the connection to close.
  var statusCode = this.statusCode;
  if ((statusCode == 204 || statusCode === 304) &&
      this.chunkedEncoding === true) {
    debug(statusCode + ' response should not use chunked encoding,' +
          ' closing connection.');
    this.chunkedEncoding = false;
    this.shouldKeepAlive = false;
  }

  // keep-alive logic
  if (state.sentConnectionHeader === false) {
    var shouldSendKeepAlive = this.shouldKeepAlive &&
        (state.sentContentLengthHeader ||
         this.useChunkedEncodingByDefault ||
         this.agent);
    if (shouldSendKeepAlive) {
      state.messageHeader += 'Connection: keep-alive\r\n';
    } else {
      this._last = true;
      state.messageHeader += 'Connection: close\r\n';
    }
  }

  if (state.sentContentLengthHeader == false &&
      state.sentTransferEncodingHeader == false) {
    if (this._hasBody) {
      if (this.useChunkedEncodingByDefault) {
        state.messageHeader += 'Transfer-Encoding: chunked\r\n';
        this.chunkedEncoding = true;
      } else {
        this._last = true;
      }
    } else {
      // Make sure we don't end the 0\r\n\r\n at the end of the message.
      this.chunkedEncoding = false;
    }
  }

  this._header = state.messageHeader + CRLF;
  this._headerSent = false;

  // wait until the first body chunk, or close(), is sent to flush,
  // UNLESS we're sending Expect: 100-continue.
  if (state.sentExpect) this._send('');
};

function storeHeader(self, state, field, value) {
  // Protect against response splitting. The if statement is there to
  // minimize the performance impact in the common case.
  if (/[\r\n]/.test(value))
    value = value.replace(/[\r\n]+[ \t]*/g, '');

  state.messageHeader += field + ': ' + value + CRLF;

  if (connectionExpression.test(field)) {
    state.sentConnectionHeader = true;
    if (closeExpression.test(value)) {
      self._last = true;
    } else {
      self.shouldKeepAlive = true;
    }

  } else if (transferEncodingExpression.test(field)) {
    state.sentTransferEncodingHeader = true;
    if (chunkExpression.test(value)) self.chunkedEncoding = true;

  } else if (contentLengthExpression.test(field)) {
    state.sentContentLengthHeader = true;
  } else if (dateExpression.test(field)) {
    state.sentDateHeader = true;
  } else if (expectExpression.test(field)) {
    state.sentExpect = true;
  }
}


OutgoingMessage.prototype.setHeader = function(name, value) {
  if (arguments.length < 2) {
    throw new Error('`name` and `value` are required for setHeader().');
  }

  if (this._header) {
    throw new Error('Can\'t set headers after they are sent.');
  }

  var key = name.toLowerCase();
  this._headers = this._headers || {};
  this._headerNames = this._headerNames || {};
  this._headers[key] = value;
  this._headerNames[key] = name;
};


OutgoingMessage.prototype.getHeader = function(name) {
  if (arguments.length < 1) {
    throw new Error('`name` is required for getHeader().');
  }

  if (!this._headers) return;

  var key = name.toLowerCase();
  return this._headers[key];
};


OutgoingMessage.prototype.removeHeader = function(name) {
  if (arguments.length < 1) {
    throw new Error('`name` is required for removeHeader().');
  }

  if (this._header) {
    throw new Error('Can\'t remove headers after they are sent.');
  }

  if (!this._headers) return;

  var key = name.toLowerCase();
  delete this._headers[key];
  delete this._headerNames[key];
};


OutgoingMessage.prototype._renderHeaders = function() {
  if (this._header) {
    throw new Error('Can\'t render headers after they are sent to the client.');
  }

  if (!this._headers) return {};

  var headers = {};
  var keys = Object.keys(this._headers);
  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    headers[this._headerNames[key]] = this._headers[key];
  }
  return headers;
};


Object.defineProperty(OutgoingMessage.prototype, 'headersSent', {
  configurable: true,
  enumerable: true,
  get: function() { return !!this._header; }
});


OutgoingMessage.prototype.write = function(chunk, encoding) {
  if (!this._header) {
    this._implicitHeader();
  }

  if (!this._hasBody) {
    debug('This type of response MUST NOT have a body. ' +
          'Ignoring write() calls.');
    return true;
  }

  if (typeof chunk !== 'string' && !Buffer.isBuffer(chunk)) {
    throw new TypeError('first argument must be a string or Buffer');
  }


  // If we get an empty string or buffer, then just do nothing, and
  // signal the user to keep writing.
  if (chunk.length === 0) return true;

  var len, ret;
  if (this.chunkedEncoding) {
    if (typeof(chunk) === 'string' &&
        encoding !== 'hex' &&
        encoding !== 'base64' &&
        encoding !== 'binary') {
      len = Buffer.byteLength(chunk, encoding);
      chunk = len.toString(16) + CRLF + chunk + CRLF;
      ret = this._send(chunk, encoding);
    } else {
      // buffer, or a non-toString-friendly encoding
      len = chunk.length;

      if (this.connection)
        this.connection.cork();
      this._send(len.toString(16));
      this._send(crlf_buf);
      this._send(chunk);
      ret = this._send(crlf_buf);
      if (this.connection)
        this.connection.uncork();
    }
  } else {
    ret = this._send(chunk, encoding);
  }

  debug('write ret = ' + ret);
  return ret;
};


OutgoingMessage.prototype.addTrailers = function(headers) {
  this._trailer = '';
  var keys = Object.keys(headers);
  var isArray = (Array.isArray(headers));
  var field, value;
  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    if (isArray) {
      field = headers[key][0];
      value = headers[key][1];
    } else {
      field = key;
      value = headers[key];
    }

    this._trailer += field + ': ' + value + CRLF;
  }
};


var zero_chunk_buf = new Buffer('\r\n0\r\n');
var crlf_buf = new Buffer('\r\n');


OutgoingMessage.prototype.end = function(data, encoding) {
  if (data && typeof data !== 'string' && !Buffer.isBuffer(data)) {
    throw new TypeError('first argument must be a string or Buffer');
  }

  if (this.finished) {
    return false;
  }
  if (!this._header) {
    this._implicitHeader();
  }

  if (data && !this._hasBody) {
    debug('This type of response MUST NOT have a body. ' +
          'Ignoring data passed to end().');
    data = false;
  }

  if (this.connection && data)
    this.connection.cork();

  var ret;
  if (data) {
    // Normal body write.
    ret = this.write(data, encoding);
  }

  if (this.chunkedEncoding) {
    ret = this._send('0\r\n' + this._trailer + '\r\n'); // Last chunk.
  } else {
    // Force a flush, HACK.
    ret = this._send('');
  }

  if (this.connection && data)
    this.connection.uncork();

  this.finished = true;

  // There is the first message on the outgoing queue, and we've sent
  // everything to the socket.
  debug('outgoing message end.');
  if (this.output.length === 0 && this.connection._httpMessage === this) {
    this._finish();
  }

  return ret;
};


OutgoingMessage.prototype._finish = function() {
  assert(this.connection);
  this.emit('finish');
};


OutgoingMessage.prototype._flush = function() {
  // This logic is probably a bit confusing. Let me explain a bit:
  //
  // In both HTTP servers and clients it is possible to queue up several
  // outgoing messages. This is easiest to imagine in the case of a client.
  // Take the following situation:
  //
  //    req1 = client.request('GET', '/');
  //    req2 = client.request('POST', '/');
  //
  // When the user does
  //
  //   req2.write('hello world\n');
  //
  // it's possible that the first request has not been completely flushed to
  // the socket yet. Thus the outgoing messages need to be prepared to queue
  // up data internally before sending it on further to the socket's queue.
  //
  // This function, outgoingFlush(), is called by both the Server and Client
  // to attempt to flush any pending messages out to the socket.

  if (!this.socket) return;

  var ret;
  while (this.output.length) {

    if (!this.socket.writable) return; // XXX Necessary?

    var data = this.output.shift();
    var encoding = this.outputEncodings.shift();

    ret = this.socket.write(data, encoding);
  }

  if (this.finished) {
    // This is a queue to the server or client to bring in the next this.
    this._finish();
  } else if (ret) {
    // This is necessary to prevent https from breaking
    this.emit('drain');
  }
};

var sys = require('sys'),
    events = require('events');

var fs = process.binding('fs');

exports.Stats = fs.Stats;

fs.Stats.prototype._checkModeProperty = function (property) {
  return ((this.mode & property) === property);
};

fs.Stats.prototype.isDirectory = function () {
  return this._checkModeProperty(process.S_IFDIR);
};

fs.Stats.prototype.isFile = function () {
  return this._checkModeProperty(process.S_IFREG);
};

fs.Stats.prototype.isBlockDevice = function () {
  return this._checkModeProperty(process.S_IFBLK);
};

fs.Stats.prototype.isCharacterDevice = function () {
  return this._checkModeProperty(process.S_IFCHR);
};

fs.Stats.prototype.isSymbolicLink = function () {
  return this._checkModeProperty(process.S_IFLNK);
};

fs.Stats.prototype.isFIFO = function () {
  return this._checkModeProperty(process.S_IFIFO);
};

fs.Stats.prototype.isSocket = function () {
  return this._checkModeProperty(process.S_IFSOCK);
};


function readAll (fd, pos, content, encoding, callback) {
  fs.read(fd, 4*1024, pos, encoding, function (err, chunk, bytesRead) {
    if (err) {
      if (callback) callback(err);
    } else if (chunk) {
      content += chunk;
      pos += bytesRead;
      readAll(fd, pos, content, encoding, callback);
    } else {
      fs.close(fd, function (err) {
        if (callback) callback(err, content);
      });
    }
  });
}

exports.readFile = function (path, encoding_, callback) {
  var encoding = typeof(encoding_) == 'string' ? encoding_ : 'utf8';
  var callback_ = arguments[arguments.length - 1];
  var callback = (typeof(callback_) == 'function' ? callback_ : null);
  fs.open(path, process.O_RDONLY, 0666, function (err, fd) {
    if (err) {
      if (callback) callback(err);
    } else {
      readAll(fd, 0, "", encoding, callback);
    }
  });
};

exports.readFileSync = function (path, encoding) {
  encoding = encoding || "utf8"; // default to utf8

  var fd = fs.open(path, process.O_RDONLY, 0666);
  var content = '';
  var pos = 0;
  var r;

  while ((r = fs.read(fd, 4*1024, pos, encoding)) && r[0]) {
    content += r[0];
    pos += r[1]
  }

  fs.close(fd);

  return content;
};


// Used by fs.open and friends
function stringToFlags(flag) {
  // Only mess with strings
  if (typeof flag !== 'string') {
    return flag;
  }
  switch (flag) {
    case "r": return process.O_RDONLY;
    case "r+": return process.O_RDWR;
    case "w": return process.O_CREAT | process.O_TRUNC | process.O_WRONLY;
    case "w+": return process.O_CREAT | process.O_TRUNC | process.O_RDWR;
    case "a": return process.O_APPEND | process.O_CREAT | process.O_WRONLY; 
    case "a+": return process.O_APPEND | process.O_CREAT | process.O_RDWR;
    default: throw new Error("Unknown file open flag: " + flag);
  }
}

function noop () {}

// Yes, the follow could be easily DRYed up but I provide the explicit
// list to make the arguments clear.

exports.close = function (fd, callback) {
  fs.close(fd, callback || noop);
};

exports.closeSync = function (fd) {
  return fs.close(fd);
};

exports.open = function (path, flags, mode, callback) {
  if (mode === undefined) { mode = 0666; }
  fs.open(path, stringToFlags(flags), mode, callback || noop);
};

exports.openSync = function (path, flags, mode) {
  if (mode === undefined) { mode = 0666; }
  return fs.open(path, stringToFlags(flags), mode);
};

exports.read = function (fd, length, position, encoding, callback) {
  encoding = encoding || "binary";
  fs.read(fd, length, position, encoding, callback || noop);
};

exports.readSync = function (fd, length, position, encoding) {
  encoding = encoding || "binary";
  return fs.read(fd, length, position, encoding);
};

exports.write = function (fd, data, position, encoding, callback) {
  encoding = encoding || "binary";
  fs.write(fd, data, position, encoding, callback || noop);
};

exports.writeSync = function (fd, data, position, encoding) {
  encoding = encoding || "binary";
  return fs.write(fd, data, position, encoding);
};

exports.rename = function (oldPath, newPath, callback) {
  fs.rename(oldPath, newPath, callback || noop);
};

exports.renameSync = function (oldPath, newPath) {
  return fs.rename(oldPath, newPath);
};

exports.truncate = function (fd, len, callback) {
  fs.truncate(fd, len, callback || noop);
};

exports.truncateSync = function (fd, len) {
  return fs.truncate(fd, len);
};

exports.rmdir = function (path, callback) {
  fs.rmdir(path, callback || noop);
};

exports.rmdirSync = function (path) {
  return fs.rmdir(path);
};

exports.mkdir = function (path, mode, callback) {
  fs.mkdir(path, mode, callback || noop);
};

exports.mkdirSync = function (path, mode) {
  return fs.mkdir(path, mode);
};

exports.sendfile = function (outFd, inFd, inOffset, length, callback) {
  fs.sendfile(outFd, inFd, inOffset, length, callback || noop);
};

exports.sendfileSync = function (outFd, inFd, inOffset, length) {
  return fs.sendfile(outFd, inFd, inOffset, length);
};

exports.readdir = function (path, callback) {
  fs.readdir(path, callback || noop);
};

exports.readdirSync = function (path) {
  return fs.readdir(path);
};

exports.lstat = function (path, callback) {
  fs.lstat(path, callback || noop);
};

exports.stat = function (path, callback) {
  fs.stat(path, callback || noop);
};

exports.lstatSync = function (path) {
  return fs.lstat(path);
};

exports.statSync = function (path) {
  return fs.stat(path);
};

exports.readlink = function (path, callback) {
  fs.readlink(path, callback || noop);
};

exports.readlinkSync = function (path) {
  return fs.readlink(path);
};

exports.symlink = function (destination, path, callback) {
  fs.symlink(destination, path, callback || noop);
};

exports.symlinkSync = function (destination, path) {
  return fs.symlink(destination, path);
};

exports.link = function (srcpath, dstpath, callback) {
  fs.link(srcpath, dstpath, callback || noop);
};

exports.linkSync = function (srcpath, dstpath) {
  return fs.link(srcpath, dstpath);
};

exports.unlink = function (path, callback) {
  fs.unlink(path, callback || noop);
};

exports.unlinkSync = function (path) {
  return fs.unlink(path);
};

exports.chmod = function (path, mode, callback) {
  fs.chmod(path, mode, callback || noop);
};

exports.chmodSync = function (path, mode) {
  return fs.chmod(path, mode);
};

function writeAll (fd, data, encoding, callback) {
  exports.write(fd, data, 0, encoding, function (writeErr, written) {
    if (writeErr) {
      exports.close(fd, function () {
        if (callback) callback(writeErr);
      });
    } else {
      if (written === data.length) {
        exports.close(fd, callback);
      } else {
        writeAll(fd, data.slice(written), encoding, callback);
      }
    }
  });
}

exports.writeFile = function (path, data, encoding_, callback) {
  var encoding = (typeof(encoding_) == 'string' ? encoding_ : 'utf8');
  var callback_ = arguments[arguments.length - 1];
  var callback = (typeof(callback_) == 'function' ? callback_ : null);
  exports.open(path, 'w', 0666, function (openErr, fd) {
    if (openErr) {
      if (callback) callback(openErr);
    } else {
      writeAll(fd, data, encoding, callback);
    }
  });
};

exports.writeFileSync = function (path, data, encoding) {
  encoding = encoding || "utf8"; // default to utf8
  var fd = exports.openSync(path, "w");
  var written = 0;
  while (written < data.length) {
    written += exports.writeSync(fd, data, 0, encoding);
    data = data.slice(written);
  }
  exports.closeSync(fd);
};

exports.cat = function () {
  throw new Error("fs.cat is deprecated. Please use fs.readFile instead.");
};


exports.catSync = function () {
  throw new Error("fs.catSync is deprecated. Please use fs.readFileSync instead.");
};

// Stat Change Watchers

var statWatchers = {};

exports.watchFile = function (filename) {
  var stat;
  var options;
  var listener;

  if ("object" == typeof arguments[1]) {
    options = arguments[1];
    listener = arguments[2];
  } else {
    options = {};
    listener = arguments[1];
  }

  if (options.persistent === undefined) options.persistent = true;
  if (options.interval === undefined) options.interval = 0;

  if (filename in statWatchers) {
    stat = statWatchers[filename];
  } else {
    statWatchers[filename] = new fs.StatWatcher();
    stat = statWatchers[filename];
    stat.start(filename, options.persistent, options.interval);
  }
  stat.addListener("change", listener);
  return stat;
};

exports.unwatchFile = function (filename) {
  if (filename in statWatchers) {
    stat = statWatchers[filename];
    stat.stop();
    statWatchers[filename] = undefined;
  }
};

// Realpath

var path = require('path');
var normalize = path.normalize
    normalizeArray = path.normalizeArray;

exports.realpathSync = function (path) {
  var seen_links = {}, knownHards = {}, buf, i = 0, part, x, stats;
  if (path.charAt(0) !== '/') {
    var cwd = process.cwd().split('/');
    path = cwd.concat(path.split('/'));
    path = normalizeArray(path);
    i = cwd.length;
    buf = [].concat(cwd);
  } else {
    path = normalizeArray(path.split('/'));
    buf = [''];
  }
  for (; i<path.length; i++) {
    part = path.slice(0, i+1).join('/');
    if (part.length !== 0) {
      if (part in knownHards) {
        buf.push(path[i]);
      } else {
        stats = exports.lstatSync(part);
        if (stats.isSymbolicLink()) {
          x = stats.dev.toString(32)+":"+stats.ino.toString(32);
          if (x in seen_links)
            throw new Error("cyclic link at "+part);
          seen_links[x] = true;
          part = exports.readlinkSync(part);
          if (part.charAt(0) === '/') {
            // absolute
            path = normalizeArray(part.split('/'));
            buf = [''];
            i = 0;
          } else {
            // relative
            Array.prototype.splice.apply(path, [i, 1].concat(part.split('/')));
            part = normalizeArray(path);
            var y = 0, L = Math.max(path.length, part.length), delta;
            for (; y<L && path[y] === part[y]; y++);
            if (y !== L) {
              path = part;
              delta = i-y;
              i = y-1;
              if (delta > 0) buf.splice(y, delta);
            } else {
              i--;
            }
          }
        } else {
          buf.push(path[i]);
          knownHards[buf.join('/')] = true;
        }
      }
    }
  }
  return buf.join('/');
}


exports.realpath = function (path, callback) {
  var seen_links = {}, knownHards = {}, buf = [''], i = 0, part, x;
  if (path.charAt(0) !== '/') {
    // assumes cwd is canonical
    var cwd = process.cwd().split('/');
    path = cwd.concat(path.split('/'));
    path = normalizeArray(path);
    i = cwd.length-1;
    buf = [].concat(cwd);
  } else {
    path = normalizeArray(path.split('/'));
  }
  function done(err) {
    if (callback) {
      if (!err) callback(err, buf.join('/'));
      else callback(err);
    }
  }
  function next() {
    if (++i === path.length) return done();
    part = path.slice(0, i+1).join('/');
    if (part.length === 0) return next();
    if (part in knownHards) {
      buf.push(path[i]);
      next();
    } else {
      exports.lstat(part, function(err, stats){
        if (err) return done(err);
        if (stats.isSymbolicLink()) {
          x = stats.dev.toString(32)+":"+stats.ino.toString(32);
          if (x in seen_links)
            return done(new Error("cyclic link at "+part));
          seen_links[x] = true;
          exports.readlink(part, function(err, npart){
            if (err) return done(err);
            part = npart;
            if (part.charAt(0) === '/') {
              // absolute
              path = normalizeArray(part.split('/'));
              buf = [''];
              i = 0;
            } else {
              // relative
              Array.prototype.splice.apply(path, [i, 1].concat(part.split('/')));
              part = normalizeArray(path);
              var y = 0, L = Math.max(path.length, part.length), delta;
              for (; y<L && path[y] === part[y]; y++);
              if (y !== L) {
                path = part;
                delta = i-y;
                i = y-1; // resolve new node if needed
                if (delta > 0) buf.splice(y, delta);
              }
              else {
                i--; // resolve new node if needed
              }
            }
            next();
          }); // fs.readlink
        }
        else {
          buf.push(path[i]);
          knownHards[buf.join('/')] = true;
          next();
        }
      }); // fs.lstat
    }
  }
  next();
}

exports.createReadStream = function(path, options) {
  return new FileReadStream(path, options);
};

var FileReadStream = exports.FileReadStream = function(path, options) {
  events.EventEmitter.call(this);

  this.path = path;
  this.fd = null;
  this.readable = true;
  this.paused = false;

  this.flags = 'r';
  this.encoding = 'binary';
  this.mode = 0666;
  this.bufferSize = 4 * 1024;

  options = options || {};
  for (var i in options) this[i] = options[i];

  var
    self = this,
    buffer = null;

  function read() {
    if (!self.readable || self.paused) {
      return;
    }

    exports.read(self.fd, self.bufferSize, undefined, self.encoding, function(err, data, bytesRead) {
      if (err) {
        self.emit('error', err);
        self.readable = false;
        return;
      }

      if (bytesRead === 0) {
        self.emit('end');
        self.forceClose();
        return;
      }

      // do not emit events if the stream is paused
      if (self.paused) {
        buffer = data;
        return;
      }

      // do not emit events anymore after we declared the stream unreadable
      if (!self.readable) {
        return;
      }

      self.emit('data', data);
      read();
    });
  }

  exports.open(this.path, this.flags, this.mode, function(err, fd) {
    if (err) {
      self.emit('error', err);
      self.readable = false;
      return;
    }

    self.fd = fd;
    self.emit('open', fd);
    read();
  });

  this.forceClose = function(cb) {
    this.readable = false;

    function close() {
      exports.close(self.fd, function(err) {
        if (err) {
          if (cb) {
            cb(err);
          }
          self.emit('error', err);
          return;
        }

        if (cb) {
          cb(null);
        }
        self.emit('close');
      });
    }

    if (this.fd) {
      close();
    } else {
      this.addListener('open', close);
    }
  };

  this.pause = function() {
    this.paused = true;
  };

  this.resume = function() {
    this.paused = false;

    if (buffer !== null) {
      self.emit('data', buffer);
      buffer = null;
    }

    read();
  };
};
sys.inherits(FileReadStream, events.EventEmitter);

exports.createWriteStream = function(path, options) {
  return new FileWriteStream(path, options);
};

var FileWriteStream = exports.FileWriteStream = function(path, options) {
  events.EventEmitter.call(this);

  this.path = path;
  this.fd = null;
  this.writeable = true;

  this.flags = 'w';
  this.encoding = 'binary';
  this.mode = 0666;

  options = options || {};
  for (var i in options) this[i] = options[i];

  var
    self = this,
    queue = [],
    busy = false;

  queue.push([exports.open, this.path, this.flags, this.mode, undefined]);

  function flush() {
    if (busy) {
      return;
    }

    var args = queue.shift();
    if (!args) {
      return self.emit('drain');
    }

    busy = true;

    var
      method = args.shift(),
      cb = args.pop();

    args.push(function(err) {
      busy = false;

      if (err) {
        self.writeable = false;
        if (cb) {
          cb(err);
        }
        self.emit('error', err);
        return;
      }

      // stop flushing after close
      if (method === exports.close) {
        if (cb) {
          cb(null);
        }
        self.emit('close');
        return;
      }

      // save reference for file pointer
      if (method === exports.open) {
        self.fd = arguments[1];
        self.emit('open', self.fd);
      } else if (cb) {
        // write callback
        cb(null, arguments[1]);
      }

      flush();
    });

    // Inject the file pointer
    if (method !== exports.open) {
      args.unshift(self.fd);
    }

    method.apply(this, args);
  };

  this.write = function(data, cb) {
    if (!this.writeable) {
      throw new Error('stream not writeable');
    }

    queue.push([exports.write, data, undefined, this.encoding, cb]);
    flush();
    return false;
  };

  this.close = function(cb) {
    this.writeable = false;
    queue.push([exports.close, cb]);
    flush();
  };

  this.forceClose = function(cb) {
    this.writeable = false;
    exports.close(self.fd, function(err) {
      if (err) {
        if (cb) {
          cb(err);
        }

        self.emit('error', err);
        return;
      }

      if (cb) {
        cb(null);
      }
      self.emit('close');
    });
  };

  flush();
};
sys.inherits(FileWriteStream, events.EventEmitter);

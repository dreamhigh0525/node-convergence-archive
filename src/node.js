(function () { // annonymous namespace

// deprecation errors

GLOBAL.include = function () {
  throw new Error("include() has been removed. Use process.mixin(process, require(file)) to get the same effect.");
};

GLOBAL.puts = function () {
  throw new Error("puts() has moved. Use require('/sys.js') to bring it back.");
}

GLOBAL.print = function () {
  throw new Error("print() has moved. Use require('/sys.js') to bring it back.");
}

GLOBAL.p = function () {
  throw new Error("p() has moved. Use require('/sys.js') to bring it back.");
}

process.debug = function () {
  throw new Error("process.debug() has moved. Use require('/sys.js') to bring it back.");
}

process.error = function () {
  throw new Error("process.error() has moved. Use require('/sys.js') to bring it back.");
}

process.tcp.createServer = function () {
  throw new Error("process.tcp.createServer() has moved. Use require('/tcp.js') to access it.");
};

process.createProcess = function () {
  throw new Error("process.createProcess() has been changed to process.createChildProcess() update your code");
};

process.exec = function () {
  throw new Error("process.exec() has moved. Use require('/sys.js') to bring it back.");
}

process.http.createServer = function () {
  throw new Error("process.http.createServer() has moved. Use require('/http.js') to access it.");
}

process.http.createClient = function () {
  throw new Error("process.http.createClient() has moved. Use require('/http.js') to access it.");
}

process.tcp.createConnection = function (port, host) {
  throw new Error("process.tcp.createConnection() has moved. Use require('/tcp.js') to access it.");
};




process.createChildProcess = function (file, args, env) {
  var child = new process.ChildProcess();
  args = args || [];
  env = env || process.ENV;
  var envPairs = [];
  for (var key in env) {
    if (env.hasOwnProperty(key)) {
      envPairs.push(key + "=" + env[key]);
    }
  }
  // TODO Note envPairs is not currently used in child_process.cc. The PATH
  // needs to be searched for the 'file' command if 'file' does not contain
  // a '/' character.
  child.spawn(file, args, envPairs);
  return child;
};

// From jQuery.extend in the jQuery JavaScript Library v1.3.2
// Copyright (c) 2009 John Resig
// Dual licensed under the MIT and GPL licenses.
// http://docs.jquery.com/License
process.mixin = function() {
	// copy reference to target object
	var target = arguments[0] || {}, i = 1, length = arguments.length, deep = false, options;

	// Handle a deep copy situation
	if ( typeof target === "boolean" ) {
		deep = target;
		target = arguments[1] || {};
		// skip the boolean and the target
		i = 2;
	}

	// Handle case when target is a string or something (possible in deep copy)
	if ( typeof target !== "object" && !process.isFunction(target) )
		target = {};

	// mixin process itself if only one argument is passed
	if ( length == i ) {
		target = GLOBAL;
		--i;
	}

	for ( ; i < length; i++ )
		// Only deal with non-null/undefined values
		if ( (options = arguments[ i ]) != null )
			// Extend the base object
			for ( var name in options ) {
				var src = target[ name ], copy = options[ name ];

				// Prevent never-ending loop
				if ( target === copy )
					continue;

				// Recurse if we're merging object values
				if ( deep && copy && typeof copy === "object" && !copy.nodeType )
					target[ name ] = process.mixin( deep, 
						// Never move original objects, clone them
						src || ( copy.length != null ? [ ] : { } )
					, copy );

				// Don't bring in undefined values
				else if ( copy !== undefined )
					target[ name ] = copy;

			}

	// Return the modified object
	return target;
};



// Signal Handlers

function isSignal (event) {
  return event.slice(0, 3) === 'SIG' && process.hasOwnProperty(event);
};

process.addListener("newListener", function (event) {
  if (isSignal(event) && process.listeners(event).length === 0) {
    var handler = new process.SignalHandler(process[event]);
    handler.addListener("signal", function () {
      process.emit(event);
    });
  }
});



// Timers

GLOBAL.setTimeout = function (callback, after) {
  var timer = new process.Timer();
  timer.addListener("timeout", callback);
  timer.start(after, 0);
  return timer;
}

GLOBAL.setInterval = function (callback, repeat) {
  var timer = new process.Timer();
  timer.addListener("timeout", callback);
  timer.start(repeat, repeat);
  return timer;
}

GLOBAL.clearTimeout = function (timer) {
  timer.stop();
}

GLOBAL.clearInterval = GLOBAL.clearTimeout;




// Modules

var debugLevel = 0;
if ("NODE_DEBUG" in process.ENV) debugLevel = 1;

function debug (x) {
  if (debugLevel > 0) {
    process.stdio.writeError(x + "\n");
  }
}


// private constructor
function Module (name, parent) {
  this.name = name;
  this.exports = {};
  this.parent = parent;

  this.filename = null;
  this.loaded = false;
  this.loadPromise = null;
  this.exited = false;
  this.children = [];
};

var moduleCache = {};

function createModule (name, parent) {
  if (name in moduleCache) {
    debug("found " + JSON.stringify(name) + " in cache");
    return moduleCache[name];
  }
  debug("didn't found " + JSON.stringify(name) + " in cache. creating new module");
  var m = new Module(name, parent);
  moduleCache[name] = m;
  return m;
};

function createInternalModule (name, constructor) {
  var m = createModule(name);
  constructor(m.exports);
  m.loaded = true;
  return m;
};

var pathModule = createInternalModule("path", function (exports) {
  exports.join = function () {
    var joined = "";
    for (var i = 0; i < arguments.length; i++) {
      var part = arguments[i].toString();

      /* Some logic to shorten paths */
      if (part === ".") continue;
      while (/^\.\//.exec(part)) part = part.replace(/^\.\//, "");

      if (i === 0) {
        part = part.replace(/\/*$/, "/");
      } else if (i === arguments.length - 1) {
        part = part.replace(/^\/*/, "");
      } else {
        part = part.replace(/^\/*/, "").replace(/\/*$/, "/");
      }
      joined += part;
    }
    return joined;
  };

  exports.dirname = function (path) {
    if (path.charAt(0) !== "/") path = "./" + path;
    var parts = path.split("/");
    return parts.slice(0, parts.length-1).join("/");
  };

  exports.filename = function (path) {
    if (path.charAt(0) !== "/") path = "./" + path;
    var parts = path.split("/");
    return parts[parts.length-1];
  };

  exports.exists = function (path, callback) {
    var p = process.fs.stat(path);
    p.addCallback(function () { callback(true); });
    p.addErrback(function () { callback(false); });
  };
});

var path = pathModule.exports;



var modulePaths = [ path.join(process.installPrefix, "lib/node/libraries")
                  ];
 
if (process.ENV["HOME"]) {
  modulePaths.unshift(path.join(process.ENV["HOME"], ".node_libraries"));
}

if (process.ENV["NODE_PATH"]) {
  modulePaths = process.ENV["NODE_PATH"].split(":").concat(modulePaths);
}


function findModulePath (name, dirs, callback) {
  process.assert(dirs.constructor == Array);

  if (/.(js|node)$/.exec(name)) {
    throw new Error("No longer accepting filename extension in module names");
  }

  if (dirs.length == 0) {
    callback();
    return;
  }
  
  var dir = dirs[0];
  var rest = dirs.slice(1, dirs.length);

  var js         = path.join(dir, name + ".js");
  var addon      = path.join(dir, name + ".node");
  var indexJs    = path.join(dir, name, "index.js");
  var indexAddon = path.join(dir, name, "index.addon");

  // TODO clean up the following atrocity!
 
  path.exists(js, function (found) {
    if (found) {
      callback(js);
      return;
    }
    path.exists(addon, function (found) {
      if (found) {
        callback(addon);
        return;
      }
      path.exists(indexJs, function (found) {
        if (found) {
          callback(indexJs);
          return;
        }
        path.exists(indexAddon, function (found) {
          if (found) {
            callback(indexAddon);
            return;
          }
          findModulePath(name, rest, callback);  
        });
      });
    });
  });
}

function loadModule (request, parent) {
  // This is the promise which is actually returned from require.async()
  var loadPromise = new process.Promise();

  loadPromise.addErrback(function (e) {
    process.stdio.writeError(e.message + "\n");
    process.exit(1);
  });

  debug("loadModule REQUEST  " + JSON.stringify(request) + " parent: " + JSON.stringify(parent));

  var name, paths;
  if (request.charAt(0) == "." && request.charAt(1) == "/") {
    // Relative request
    name = path.join(path.dirname(parent.name), request);
    paths = [path.dirname(parent.filename)];
  } else {
    name = request;
    paths = modulePaths;
  }

  if (name in moduleCache) {
    debug("found  " + JSON.stringify(name) + " in cache");
    // In cache
    var module = moduleCache[name];
    setTimeout(function () {
      loadPromise.emitSuccess(module.exports);
    }, 0);
  } else {
    debug("looking for " + JSON.stringify(name) + " in " + JSON.stringify(paths));
    // Not in cache
    findModulePath(request, paths, function (filename) {
      if (!filename) {
        loadPromise.emitError(new Error("Cannot find module '" + request + "'"));
      } else {
        var module = createModule(name, parent); 
        module.load(filename, loadPromise);
      }
    });
  }

  return loadPromise;
};

Module.prototype.load = function (filename, loadPromise) {
  debug("load " + JSON.stringify(filename) + " for module " + JSON.stringify(this.name));

  process.assert(!this.loaded);
  process.assert(!this.loadPromise);

  this.loadPromise = loadPromise;
  this.filename = filename;

  if (filename.match(/\.node$/)) {
    this.loadObject(filename, loadPromise);
  } else {
    this.loadScript(filename, loadPromise);
  }
};

Module.prototype.loadObject = function (filename, loadPromise) {
  var self = this;
  // XXX Not yet supporting loading from HTTP. would need to download the
  // file, store it to tmp then run dlopen on it. 
  setTimeout(function () {
    self.loaded = true;
    process.dlopen(filename, self.exports); // FIXME synchronus
    loadPromise.emitSuccess(self.exports);
  }, 0);
};

Module.prototype.loadScript = function (filename, loadPromise) {
  var self = this;
  var catPromise = process.cat(filename);

  catPromise.addErrback(function () {
    loadPromise.emitError(new Error("Error reading " + filename));
  });

  catPromise.addCallback(function (content) {
    // remove shebang
    content = content.replace(/^\#\!.*/, '');

    function requireAsync (url) {
      return loadModule(url, self); // new child
    }

    function require (url) {
      return requireAsync(url).wait();
    }

    require.paths = modulePaths;
    require.async = requireAsync;

    // create wrapper function
    var wrapper = "var __wrap__ = function (exports, require, module, __filename) { " 
                + content 
                + "\n}; __wrap__;";
    var compiledWrapper = process.compile(wrapper, filename);

    compiledWrapper.apply(self.exports, [self.exports, require, self, filename]);

    self.waitChildrenLoad(function () {
      self.loaded = true;
      loadPromise.emitSuccess(self.exports);
    });
  });
};

Module.prototype.waitChildrenLoad = function (callback) {
  var nloaded = 0;
  var children = this.children;
  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    if (child.loaded) {
      nloaded++;
    } else {
      child.loadPromise.addCallback(function () {
        nloaded++;
        if (children.length == nloaded && callback) callback();
      });
    }
  }
  if (children.length == nloaded && callback) callback();
};


process.exit = function (code) {
  process.emit("exit");
  process.reallyExit(code);
};

var cwd = process.cwd();

// Make process.ARGV[0] and process.ARGV[1] into full paths.
if (process.ARGV[0].charAt(0) != "/") {
  process.ARGV[0] = path.join(cwd, process.ARGV[0]);
}

if (process.ARGV[1].charAt(0) != "/") {
  process.ARGV[1] = path.join(cwd, process.ARGV[1]);
}

// Load the root module--the command line argument.
var m = createModule("."); 
var loadPromise = new process.Promise();
m.load(process.ARGV[1], loadPromise);
loadPromise.wait();

}()); // end annonymous namespace

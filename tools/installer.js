var fs = require('fs'),
    path = require('path'),
    exec = require('child_process').exec,
    options = fs.readFileSync(process.argv[2]).toString(),
    cmd = process.argv[3];

if (cmd !== 'install' && cmd !== 'uninstall') {
  console.error('Unknown command: ' + cmd);
  process.exit(1);
}

// Parse options file and remove first comment line
options = JSON.parse(options.split('\n').slice(1).join(''));
var variables = options.variables,
    node_prefix = variables.node_prefix || '/usr/local';

// Execution queue
var queue = [],
    dirs = [];

// Copy file from src to dst
function copy(src, dst, callback) {
  // If src is array - copy each file separately
  if (Array.isArray(src)) {
    src.forEach(function(src) {
      copy(src, dst, callback);
    });
    return;
  }

  dst = path.join(node_prefix, dst);
  var dir = dst.replace(/\/[^\/]*$/, '/');

  // Create directory if hasn't done this yet
  if (dirs.indexOf(dir) === -1) {
    dirs.push(dir);
    queue.push('mkdir -p ' + dir);
  }

  // Queue file/dir copy
  queue.push('cp -rf ' + src + ' ' + dst);
}

// Remove files
function remove(files) {
  files.forEach(function(file) {
    file = path.join(node_prefix, file);
    queue.push('rm -rf ' + file);
  });
}

// Run every command in queue, one-by-one
function run() {
  var cmd = queue.shift();
  if (!cmd) return;

  console.log(cmd);
  exec(cmd, function(err, stdout, stderr) {
    if (stderr) console.error(stderr);
    if (err) process.exit(1);

    run();
  });
}

if (cmd === 'install') {
  // Copy includes
  copy([
    // Node
    'src/node.h', 'src/node_buffer.h', 'src/node_object_wrap.h',
    'src/node_version.h',
    // v8
    'deps/v8/include/v8-debug.h', 'deps/v8/include/v8-preparser.h',
    'deps/v8/include/v8-profiler.h', 'deps/v8/include/v8-testing.h',
    'deps/v8/include/v8.h', 'deps/v8/include/v8stdint.h',
    // uv
    'deps/uv/include/uv.h'
  ], 'include/node/');

  // Private uv headers
  copy([
    'deps/uv/include/uv-private/eio.h', 'deps/uv/include/uv-private/ev.h',
    'deps/uv/include/uv-private/ngx-queue.h',
    'deps/uv/include/uv-private/tree.h',
    'deps/uv/include/uv-private/uv-unix.h',
    'deps/uv/include/uv-private/uv-win.h',
  ], 'include/node/uv-private/');

  copy([
    'deps/uv/include/ares.h',
    'deps/uv/include/ares_version.h'
  ], 'include/node/c-ares/');

  // Copy binary file
  copy('out/Release/node', 'bin/node');

  // Install npm (eventually)
  if (variables.node_install_npm == 'true') {
    copy('deps/npm', 'lib/node_modules/npm');
    queue.push('ln -sF ../lib/node_modules/npm/bin/npm-cli.js ' +
               path.join(node_prefix, 'bin/npm'));
  }
} else {
  remove([
     'bin/node', 'bin/npm', 'include/node/*', 'lib/node_modules'
  ]);
}

run();

{
  'variables': {
    'v8_use_snapshot': 'true',
    'target_arch': 'ia32',
    'node_use_dtrace': 'false',
    'node_use_openssl%': 'true'
  },

  'targets': [
    {
      'target_name': 'node',
      'type': 'executable',

      'dependencies': [
        'deps/http_parser/http_parser.gyp:http_parser',
        'deps/v8/tools/gyp/v8-node.gyp:v8',
        'deps/uv/uv.gyp:uv',
        'node_js2c#host',
      ],

      'include_dirs': [
        'src',
        'deps/uv/src/ares',
        '<(SHARED_INTERMEDIATE_DIR)' # for node_natives.h
      ],

      'sources': [
        'src/cares_wrap.cc',
        'src/handle_wrap.cc',
        'src/node.cc',
        'src/node_buffer.cc',
        'src/node_constants.cc',
        'src/node_dtrace.cc',
        'src/node_extensions.cc',
        'src/node_file.cc',
        'src/node_http_parser.cc',
        'src/node_javascript.cc',
        'src/node_main.cc',
        'src/node_os.cc',
        'src/node_script.cc',
        'src/node_string.cc',
        'src/pipe_wrap.cc',
        'src/stdio_wrap.cc',
        'src/stream_wrap.cc',
        'src/tcp_wrap.cc',
        'src/timer_wrap.cc',
        'src/process_wrap.cc',
        'src/v8_typed_array.cc',
      ],

      'defines': [
        'ARCH="<(target_arch)"',
        'PLATFORM="<(OS)"',
        '_LARGEFILE_SOURCE',
        '_FILE_OFFSET_BITS=64',
      ],

      'conditions': [
        [ 'node_use_openssl=="true"', {
          'defines': [ 'HAVE_OPENSSL=1' ],
          'sources': [ 'src/node_crypto.cc' ],
          'dependencies': [ './deps/openssl/openssl.gyp:openssl' ]
        }, {
          'defines': [ 'HAVE_OPENSSL=0' ]
        }],

        [ 'OS=="win"', {
          'dependencies': [
            'deps/uv/deps/pthread-win32/pthread-win32.gyp:pthread-win32',
          ],
          'sources': [
            'src/platform_win32.cc',
            'src/node_stdio_win32.cc',
            # file operations depend on eio to link. uv contains eio in unix builds, but not win32. So we need to compile it here instead.
            'deps/uv/src/eio/eio.c',
          ],
          'defines': [
            'PTW32_STATIC_LIB',
            'FD_SETSIZE=1024',
            # we need to use node's preferred "win32" rather than gyp's preferred "win"
            'PLATFORM="win32"',
          ],
        },{ # POSIX
          'defines': [ '__POSIX__' ],
          'sources': [
            'src/node_cares.cc',
            'src/node_net.cc',
            'src/node_signal_watcher.cc',
            'src/node_stat_watcher.cc',
            'src/node_io_watcher.cc',
            'src/node_stdio.cc',
            'src/node_child_process.cc',
            'src/node_timer.cc'
          ]
        }],
        [ 'OS=="mac"', {
          'sources': [ 'src/platform_darwin.cc' ],
          'libraries': [ '-framework Carbon' ],
        }],
        [ 'OS=="linux"', {
          'sources': [ 'src/platform_linux.cc' ],
          'libraries': [
            '-lutil' # needed for openpty
          ],
        }]
      ],
      'msvs-settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # /subsystem:console
        },
      },
    },

    {
      'target_name': 'node_js2c',
      'type': 'none',
      'toolsets': ['host'],
      'variables': {
        'library_files': [
          'src/node.js',
          'lib/_debugger.js',
          'lib/_linklist.js',
          'lib/assert.js',
          'lib/buffer.js',
          'lib/buffer_ieee754.js',
          'lib/child_process_legacy.js',
          'lib/child_process_uv.js',
          'lib/console.js',
          'lib/constants.js',
          'lib/crypto.js',
          'lib/dgram.js',
          'lib/dns_legacy.js',
          'lib/dns_uv.js',
          'lib/events.js',
          'lib/freelist.js',
          'lib/fs.js',
          'lib/http.js',
          'lib/http2.js',
          'lib/https.js',
          'lib/https2.js',
          'lib/module.js',
          'lib/net_legacy.js',
          'lib/net_uv.js',
          'lib/os.js',
          'lib/path.js',
          'lib/punycode.js',
          'lib/querystring.js',
          'lib/readline.js',
          'lib/repl.js',
          'lib/stream.js',
          'lib/string_decoder.js',
          'lib/sys.js',
          'lib/timers_legacy.js',
          'lib/timers_uv.js',
          'lib/tls.js',
          'lib/tty.js',
          'lib/tty_posix.js',
          'lib/tty_win32.js',
          'lib/url.js',
          'lib/util.js',
          'lib/vm.js',
        ],
      },

      'actions': [
        {
          'action_name': 'node_js2c',

          'inputs': [
            './tools/js2c.py',
            '<@(library_files)',
          ],

          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/node_natives.h',
          ],

          # FIXME can the following conditions be shorted by just setting
          # macros.py into some variable which then gets included in the
          # action?

          'conditions': [
            [ 'node_use_dtrace=="true"', {
              'action': [
                'python',
                'tools/js2c.py',
                '<@(_outputs)',
                '<@(library_files)'
              ],
            }, { # No Dtrace
              'action': [
                'python',
                'tools/js2c.py',
                '<@(_outputs)',
                '<@(library_files)',
                'src/macros.py'
              ],
            }]
          ],
        },
      ],
    }, # end node_js2c
  ] # end targets
}


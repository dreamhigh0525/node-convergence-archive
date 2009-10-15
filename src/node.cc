// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> /* PATH_MAX */
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h> /* dlopen(), dlsym() */

#include <events.h>
#include <dns.h>
#include <net.h>
#include <file.h>
#include <http.h>
#include <signal_handler.h>
#include <timer.h>
#include <child_process.h>
#include <constants.h>
#include <node_stdio.h>
#include <node_natives.h>
#include <v8-debug.h>
#include <node_version.h>

using namespace v8;

extern char **environ;

namespace node {

static int dash_dash_index = 0;
static bool use_debug_agent = false;

enum encoding ParseEncoding(Handle<Value> encoding_v, enum encoding _default) {
  HandleScope scope;

  if (!encoding_v->IsString()) return _default;

  String::Utf8Value encoding(encoding_v->ToString());

  if (strcasecmp(*encoding, "utf8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "utf-8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "ascii") == 0) {
    return ASCII;
  } else if (strcasecmp(*encoding, "binary") == 0) {
    return BINARY;
  } else if (strcasecmp(*encoding, "raw") == 0) {
    fprintf(stderr, "'raw' (array of integers) has been removed. "
                    "Use 'binary'.\n");
    return BINARY;
  } else if (strcasecmp(*encoding, "raws") == 0) {
    fprintf(stderr, "'raws' encoding has been renamed to 'binary'. "
                    "Please update your code.\n");
    return BINARY;
  } else {
    return _default;
  }
}

Local<Value> Encode(const void *buf, size_t len, enum encoding encoding) {
  HandleScope scope;

  if (!len) return scope.Close(Null());

  if (encoding == BINARY) {
    const unsigned char *cbuf = static_cast<const unsigned char*>(buf);
    uint16_t * twobytebuf = new uint16_t[len];
    for (size_t i = 0; i < len; i++) {
      // XXX is the following line platform independent?
      twobytebuf[i] = cbuf[i];
    }
    Local<String> chunk = String::New(twobytebuf, len);
    delete [] twobytebuf; // TODO use ExternalTwoByteString?
    return scope.Close(chunk);
  }

  // utf8 or ascii encoding
  Local<String> chunk = String::New((const char*)buf, len);
  return scope.Close(chunk);
}

// Returns -1 if the handle was not valid for decoding
ssize_t DecodeBytes(v8::Handle<v8::Value> val, enum encoding encoding) {
  HandleScope scope;

  if (val->IsArray()) {
    fprintf(stderr, "'raw' encoding (array of integers) has been removed. "
                    "Use 'binary'.\n");
    assert(0);
    return -1;
  }

  Local<String> str = val->ToString();

  if (encoding == UTF8) return str->Utf8Length();

  return str->Length();
}

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Returns number of bytes written.
ssize_t DecodeWrite(char *buf, size_t buflen,
                    v8::Handle<v8::Value> val,
                    enum encoding encoding) {
  size_t i;
  HandleScope scope;

  // XXX
  // A lot of improvement can be made here. See:
  // http://code.google.com/p/v8/issues/detail?id=270
  // http://groups.google.com/group/v8-dev/browse_thread/thread/dba28a81d9215291/ece2b50a3b4022c
  // http://groups.google.com/group/v8-users/browse_thread/thread/1f83b0ba1f0a611

  if (val->IsArray()) {
    fprintf(stderr, "'raw' encoding (array of integers) has been removed. "
                    "Use 'binary'.\n");
    assert(0);
    return -1;
  }

  Local<String> str = val->ToString();

  if (encoding == UTF8) {
    str->WriteUtf8(buf, buflen);
    return buflen;
  }

  if (encoding == ASCII) {
    str->WriteAscii(buf, 0, buflen);
    return buflen;
  }

  // THIS IS AWFUL!!! FIXME

  assert(encoding == BINARY);

  uint16_t * twobytebuf = new uint16_t[buflen];

  str->Write(twobytebuf, 0, buflen);

  for (size_t i = 0; i < buflen; i++) {
    unsigned char *b = reinterpret_cast<unsigned char*>(&twobytebuf[i]);
    assert(b[1] == 0);
    buf[i] = b[0];
  }

  delete [] twobytebuf;

  return buflen;
}

// Extracts a C str from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<str conversion failed>";
}

static void ReportException(TryCatch *try_catch) {
  Handle<Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    fprintf(stderr, "Error: (no message)\n");
    fflush(stderr);
    return;
  }

  Handle<Value> error = try_catch->Exception();
  Handle<String> stack;

  if (error->IsObject()) {
    Handle<Object> obj = Handle<Object>::Cast(error);
    Handle<Value> raw_stack = obj->Get(String::New("stack"));
    if (raw_stack->IsString()) stack = Handle<String>::Cast(raw_stack);
  }

  // Print (filename):(line number): (message).
  String::Utf8Value filename(message->GetScriptResourceName());
  const char* filename_string = ToCString(filename);
  int linenum = message->GetLineNumber();
  fprintf(stderr, "%s:%i\n", filename_string, linenum);
  // Print line of source code.
  String::Utf8Value sourceline(message->GetSourceLine());
  const char* sourceline_string = ToCString(sourceline);
  fprintf(stderr, "%s\n", sourceline_string);
  // Print wavy underline (GetUnderline is deprecated).
  int start = message->GetStartColumn();
  for (int i = 0; i < start; i++) {
    fprintf(stderr, " ");
  }
  int end = message->GetEndColumn();
  for (int i = start; i < end; i++) {
    fprintf(stderr, "^");
  }
  fprintf(stderr, "\n");

  if (stack.IsEmpty()) {
    message->PrintCurrentStackTrace(stderr);
  } else {
    String::Utf8Value trace(stack);
    fprintf(stderr, "%s\n", *trace);
  }
  fflush(stderr);
}

// Executes a str within the current v8 context.
Handle<Value> ExecuteString(v8::Handle<v8::String> source,
                            v8::Handle<v8::Value> filename) {
  HandleScope scope;
  TryCatch try_catch;

  Handle<Script> script = Script::Compile(source, filename);
  if (script.IsEmpty()) {
    ReportException(&try_catch);
    exit(1);
  }

  Handle<Value> result = script->Run();
  if (result.IsEmpty()) {
    ReportException(&try_catch);
    exit(1);
  }

  return scope.Close(result);
}

static Handle<Value> Cwd(const Arguments& args) {
  HandleScope scope;

  char output[PATH_MAX];
  char *r = getcwd(output, PATH_MAX);
  if (r == NULL) {
    return ThrowException(Exception::Error(String::New(strerror(errno))));
  }
  Local<String> cwd = String::New(output);

  return scope.Close(cwd);
}

v8::Handle<v8::Value> Exit(const v8::Arguments& args) {
  int r = 0;
  if (args.Length() > 0)
    r = args[0]->IntegerValue();
  fflush(stderr);
  exit(r);
  return Undefined();
}

v8::Handle<v8::Value> Kill(const v8::Arguments& args) {
  HandleScope scope;
  
  if (args.Length() < 1 || !args[0]->IsNumber()) {
    return ThrowException(Exception::Error(String::New("Bad argument.")));
  }
  
  pid_t pid = args[0]->IntegerValue();

  int sig = SIGTERM;

  if (args.Length() >= 2) {
    if (args[1]->IsNumber()) {
      sig = args[1]->Int32Value();
    } else if (args[1]->IsString()) {
      Local<String> signame = args[1]->ToString();
      Local<Object> process = Context::GetCurrent()->Global();
      Local<Object> node_obj = process->Get(String::NewSymbol("node"))->ToObject();

      Local<Value> sig_v = node_obj->Get(signame);
      if (!sig_v->IsNumber()) {
        return ThrowException(Exception::Error(String::New("Unknown signal")));
      }
      sig = sig_v->Int32Value();
    }
  }

  int r = kill(pid, sig);

  if (r != 0) {
    return ThrowException(Exception::Error(String::New(strerror(errno))));
  }

  return Undefined();
}

typedef void (*extInit)(Handle<Object> exports);

// DLOpen is node.dlopen(). Used to load 'module.node' dynamically shared
// objects.
Handle<Value> DLOpen(const v8::Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) return Undefined();

  String::Utf8Value filename(args[0]->ToString()); // Cast
  Local<Object> target = args[1]->ToObject(); // Cast

  // Actually call dlopen().
  // FIXME: This is a blocking function and should be called asynchronously!
  // This function should be moved to file.cc and use libeio to make this
  // system call.
  void *handle = dlopen(*filename, RTLD_LAZY);

  // Handle errors.
  if (handle == NULL) {
    Local<Value> exception = Exception::Error(String::New(dlerror()));
    return ThrowException(exception);
  }

  // Get the init() function from the dynamically shared object.
  void *init_handle = dlsym(handle, "init");
  // Error out if not found.
  if (init_handle == NULL) {
    Local<Value> exception =
      Exception::Error(String::New("No 'init' symbol found in module."));
    return ThrowException(exception);
  }
  extInit init = (extInit)(init_handle); // Cast

  // Execute the C++ module
  init(target);

  return Undefined();
}

v8::Handle<v8::Value> Compile(const v8::Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
          String::New("needs two arguments.")));
  }

  Local<String> source = args[0]->ToString();
  Local<String> filename = args[1]->ToString();

  Handle<Script> script = Script::Compile(source, filename);
  if (script.IsEmpty()) return Undefined();

  Handle<Value> result = script->Run();
  if (result.IsEmpty()) return Undefined();

  return scope.Close(result);
}

static void OnFatalError(const char* location, const char* message) {
#define FATAL_ERROR "\033[1;31mV8 FATAL ERROR.\033[m"
  if (location)
    fprintf(stderr, FATAL_ERROR " %s %s\n", location, message);
  else
    fprintf(stderr, FATAL_ERROR " %s\n", message);

  exit(1);
}

void FatalException(TryCatch &try_catch) {
  ReportException(&try_catch);
  exit(1);
}

static ev_async eio_watcher;

// Called from the main thread.
static void EIOCallback(EV_P_ ev_async *watcher, int revents) {
  assert(watcher == &eio_watcher);
  assert(revents == EV_ASYNC);
  // Give control to EIO to process responses. In nearly every case
  // EIOPromise::After() (file.cc) is called once EIO receives the response.
  eio_poll();
}

// EIOWantPoll() is called from the EIO thread pool each time an EIO
// request (that is, one of the node.fs.* functions) has completed.
static void EIOWantPoll(void) {
  // Signal the main thread that EIO callbacks need to be processed.
  ev_async_send(EV_DEFAULT_UC_ &eio_watcher);
  // EIOCallback() will be called from the main thread in the next event
  // loop.
}

static ev_async debug_watcher;

static void DebugMessageCallback(EV_P_ ev_async *watcher, int revents) {
  HandleScope scope;
  assert(watcher == &debug_watcher);
  assert(revents == EV_ASYNC);
  ExecuteString(String::New("1+1;"),
                String::New("debug_poll"));
}

static void DebugMessageDispatch(void) {
  // This function is called from V8's debug thread when a debug TCP client
  // has sent a message.

  // Send a signal to our main thread saying that it should enter V8 to
  // handle the message.
  ev_async_send(EV_DEFAULT_UC_ &debug_watcher);
}


static void ExecuteNativeJS(const char *filename, const char *data) {
  HandleScope scope;
  TryCatch try_catch;
  ExecuteString(String::New(data), String::New(filename));
  // There should not be any syntax errors in these file!
  // If there are exit the process.
  if (try_catch.HasCaught())  {
    puts("There is an error in Node's built-in javascript");
    puts("This should be reported as a bug!");
    ReportException(&try_catch);
    exit(1);
  }
}

static Local<Object> Load(int argc, char *argv[]) {
  HandleScope scope;

  // Reference to 'process'
  Local<Object> process = Context::GetCurrent()->Global();

  Local<Object> node_obj = Object::New(); // Create the 'process.node' object
  process->Set(String::NewSymbol("node"), node_obj); // and assign it.

  // node.version
  node_obj->Set(String::NewSymbol("version"), String::New(NODE_VERSION));
  // node.installPrefix
  node_obj->Set(String::NewSymbol("installPrefix"), String::New(NODE_PREFIX));

  // process.ARGV
  int i, j;
  Local<Array> arguments = Array::New(argc - dash_dash_index + 1);
  arguments->Set(Integer::New(0), String::New(argv[0]));
  for (j = 1, i = dash_dash_index + 1; i < argc; j++, i++) {
    Local<String> arg = String::New(argv[i]);
    arguments->Set(Integer::New(j), arg);
  }
  // assign it
  process->Set(String::NewSymbol("ARGV"), arguments);

  // create process.ENV
  Local<Object> env = Object::New();
  for (i = 0; environ[i]; i++) {
    // skip entries without a '=' character
    for (j = 0; environ[i][j] && environ[i][j] != '='; j++) { ; }
    // create the v8 objects
    Local<String> field = String::New(environ[i], j);
    Local<String> value = Local<String>();
    if (environ[i][j] == '=') {
      value = String::New(environ[i]+j+1);
    }
    // assign them
    env->Set(field, value);
  }
  // assign process.ENV
  process->Set(String::NewSymbol("ENV"), env);
  process->Set(String::NewSymbol("pid"), Integer::New(getpid()));

  // define various internal methods
  NODE_SET_METHOD(node_obj, "compile", Compile);
  NODE_SET_METHOD(node_obj, "reallyExit", Exit);
  NODE_SET_METHOD(node_obj, "cwd", Cwd);
  NODE_SET_METHOD(node_obj, "dlopen", DLOpen);
  NODE_SET_METHOD(node_obj, "kill", Kill);

  // Assign the EventEmitter. It was created in main().
  node_obj->Set(String::NewSymbol("EventEmitter"),
              EventEmitter::constructor_template->GetFunction());

  // Initialize the C++ modules..................filename of module
  Promise::Initialize(node_obj);                // events.cc
  Stdio::Initialize(node_obj);                  // stdio.cc
  Timer::Initialize(node_obj);                  // timer.cc
  SignalHandler::Initialize(node_obj);          // signal_handler.cc
  ChildProcess::Initialize(node_obj);           // child_process.cc
  DefineConstants(node_obj);                    // constants.cc
  // Create node.dns
  Local<Object> dns = Object::New();
  node_obj->Set(String::NewSymbol("dns"), dns);
  DNS::Initialize(dns);                         // dns.cc
  Local<Object> fs = Object::New();
  node_obj->Set(String::NewSymbol("fs"), fs);
  File::Initialize(fs);                         // file.cc
  // Create node.tcp. Note this separate from lib/tcp.js which is the public
  // frontend.
  Local<Object> tcp = Object::New();
  node_obj->Set(String::New("tcp"), tcp);
  Server::Initialize(tcp);                      // tcp.cc
  Connection::Initialize(tcp);                  // tcp.cc
  // Create node.http.  Note this separate from lib/http.js which is the
  // public frontend.
  Local<Object> http = Object::New();
  node_obj->Set(String::New("http"), http);
  HTTPServer::Initialize(http);                 // http.cc
  HTTPConnection::Initialize(http);             // http.cc

  // Compile, execute the src/*.js files. (Which were included a static C
  // strings in node_natives.h)
  ExecuteNativeJS("util.js", native_util);
  ExecuteNativeJS("events.js", native_events);
  ExecuteNativeJS("file.js", native_file);
  // In node.js we actually load the file specified in ARGV[1]
  // so your next reading stop should be node.js!
  ExecuteNativeJS("node.js", native_node);

  return scope.Close(node_obj);
}

static void EmitExitEvent() {
  HandleScope scope;

  // Get reference to 'process' object.
  Local<Object> process = Context::GetCurrent()->Global();
  // Get the 'emit' function from it.
  Local<Value> emit_v = process->Get(String::NewSymbol("emit"));
  if (!emit_v->IsFunction()) {
    // could not emit exit event so exit
    exit(10);
  }
  // Cast
  Local<Function> emit = Local<Function>::Cast(emit_v);

  TryCatch try_catch;

  // Arguments for the emit('exit')
  Local<Value> argv[2] = { String::New("exit"), Integer::New(0) };
  // Emit!
  emit->Call(process, 2, argv);

  if (try_catch.HasCaught()) {
    node::FatalException(try_catch);
  }
}

static void PrintHelp() {
  printf("Usage: node [options] [--] script.js [arguments] \n"
         "  -v, --version    print node's version\n"
         "  --debug          enable remote debugging\n" // TODO specify port
         "  --cflags         print pre-processor and compiler flags\n"
         "  --v8-options     print v8 command line options\n\n"
         "Documentation can be found at http://tinyclouds.org/node/api.html"
         " or with 'man node'\n");
}

// Parse node command line arguments.
static void ParseArgs(int *argc, char **argv) {
  // TODO use parse opts
  for (int i = 1; i < *argc; i++) {
    const char *arg = argv[i];
    if (strcmp(arg, "--") == 0) {
      dash_dash_index = i;
      break;
    } else if (strcmp(arg, "--debug") == 0) {
      argv[i] = reinterpret_cast<const char*>("");
      use_debug_agent = true;
      dash_dash_index = i;
    } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("%s\n", NODE_VERSION);
      exit(0);
    } else if (strcmp(arg, "--cflags") == 0) {
      printf("%s\n", NODE_CFLAGS);
      exit(0);
    } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      PrintHelp();
      exit(0);
    } else if (strcmp(arg, "--v8-options") == 0) {
      argv[i] = reinterpret_cast<const char*>("--help");
      dash_dash_index = i+1;
    }
  }
}

}  // namespace node


int main(int argc, char *argv[]) {
  // Parse a few arguments which are specific to Node.
  node::ParseArgs(&argc, argv);
  // Parse the rest of the args (up to the 'dash_dash_index' (where '--' was
  // in the command line))
  V8::SetFlagsFromCommandLine(&node::dash_dash_index, argv, false);

  // Error out if we don't have a script argument.
  if (argc < 2) {
    fprintf(stderr, "No script was specified.\n");
    node::PrintHelp();
    return 1;
  }

  // Ignore the SIGPIPE
  evcom_ignore_sigpipe();

  // Initialize the default ev loop.
  ev_default_loop(EVFLAG_AUTO);

  // Start the EIO thread pool:
  // 1. Initialize the ev_async watcher which allows for notification from
  // the thread pool (in node::EIOWantPoll) to poll for updates (in
  // node::EIOCallback).
  ev_async_init(&node::eio_watcher, node::EIOCallback);
  // 2. Actaully start the thread pool.
  eio_init(node::EIOWantPoll, NULL);
  // 3. Start watcher.
  ev_async_start(EV_DEFAULT_UC_ &node::eio_watcher);
  // 4. Remove a reference to the async watcher. This means we'll drop out
  // of the ev_loop even though eio_watcher is active.
  ev_unref(EV_DEFAULT_UC);

  V8::Initialize();
  HandleScope handle_scope;

  V8::SetFatalErrorHandler(node::OnFatalError);

#define AUTO_BREAK_FLAG "--debugger_auto_break"
  // If the --debug flag was specified then initialize the debug thread.
  if (node::use_debug_agent) {
    // First apply --debugger_auto_break setting to V8. This is so we can
    // enter V8 by just executing any bit of javascript
    V8::SetFlagsFromString(AUTO_BREAK_FLAG, sizeof(AUTO_BREAK_FLAG));
    // Initialize the async watcher for receiving messages from the debug
    // thread and marshal it into the main thread. DebugMessageCallback()
    // is called from the main thread to execute a random bit of javascript
    // - which will give V8 control so it can handle whatever new message
    // had been received on the debug thread.
    ev_async_init(&node::debug_watcher, node::DebugMessageCallback);
    // Set the callback DebugMessageDispatch which is called from the debug
    // thread.
    Debug::SetDebugMessageDispatchHandler(node::DebugMessageDispatch);
    // Start the async watcher.
    ev_async_start(EV_DEFAULT_UC_ &node::debug_watcher);
    // unref it so that we exit the event loop despite it being active.
    ev_unref(EV_DEFAULT_UC);

    // Start the debug thread and it's associated TCP server on port 5858.
    bool r = Debug::EnableAgent("node " NODE_VERSION, 5858);
    // Crappy check that everything went well. FIXME
    assert(r);
    // Print out some information. REMOVEME
    printf("debugger listening on port 5858\n"
           "Use 'd8 --remote_debugger' to access it.\n");
  }

  // Create the global 'process' object's FunctionTemplate.
  Local<FunctionTemplate> process_template = FunctionTemplate::New();

  // The global object (process) is an instance of EventEmitter. For some
  // strange and forgotten reasons we must initialize EventEmitter now
  // before creating the Context. EventEmitter will be assigned to it's
  // namespace node.EventEmitter in Load() bellow.
  node::EventEmitter::Initialize(process_template);

  // Create the one and only Context.
  Persistent<Context> context = Context::New(NULL,
      process_template->InstanceTemplate());
  Context::Scope context_scope(context);

  // Actually assign the global object to it's place as 'process'
  context->Global()->Set(String::NewSymbol("process"), context->Global());

  // Create all the objects, load modules, do everything.
  // so your next reading stop should be node::Load()!
  Local<Object> node_obj = node::Load(argc, argv);

  // All our arguments are loaded. We've evaluated all of the scripts. We
  // might even have created TCP servers. Now we enter the main event loop.
  // If there are no watchers on the loop (except for the ones that were
  // ev_unref'd) then this function exits. As long as there are active
  // watchers, it blocks.
  ev_loop(EV_DEFAULT_UC_ 0);  // main event loop

  // Once we've dropped out, emit the 'exit' event from 'process'
  node::EmitExitEvent();

#ifndef NDEBUG
  printf("clean up\n");
  // Clean up.
  context.Dispose();
  V8::Dispose();
#endif  // NDEBUG
  return 0;
}


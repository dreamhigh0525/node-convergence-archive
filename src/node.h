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

#ifndef SRC_NODE_H_
#define SRC_NODE_H_

#ifdef _WIN32
# ifndef BUILDING_NODE_EXTENSION
#   define NODE_EXTERN __declspec(dllexport)
# else
#   define NODE_EXTERN __declspec(dllimport)
# endif
#else
# define NODE_EXTERN /* nothing */
#endif

#ifdef BUILDING_NODE_EXTENSION
# undef BUILDING_V8_SHARED
# undef BUILDING_UV_SHARED
# define USING_V8_SHARED 1
# define USING_UV_SHARED 1
#endif

// This should be defined in make system.
// See issue https://github.com/joyent/node/issues/1236
#if defined(__MINGW32__) || defined(_MSC_VER)
#ifndef _WIN32_WINNT
# define _WIN32_WINNT   0x0501
#endif

#define NOMINMAX

#endif

#if defined(_MSC_VER)
#define PATH_MAX MAX_PATH
#endif

#ifdef _WIN32
# define SIGKILL         9
#endif

#include "uv.h"
#include "v8.h"
#include <sys/types.h> /* struct stat */
#include <sys/stat.h>
#include <assert.h>

#include "node_object_wrap.h"

// Forward-declare these functions now to stop MSVS from becoming
// terminally confused when it's done in node_internals.h
namespace node {

NODE_EXTERN v8::Local<v8::Value> ErrnoException(int errorno,
                                                const char* syscall = NULL,
                                                const char* message = NULL,
                                                const char* path = NULL);
NODE_EXTERN v8::Local<v8::Value> UVException(int errorno,
                                             const char* syscall = NULL,
                                             const char* message = NULL,
                                             const char* path = NULL);
NODE_EXTERN v8::Handle<v8::Value> MakeCallback(
    const v8::Handle<v8::Object> recv,
    const char* method,
    int argc,
    v8::Handle<v8::Value>* argv);
NODE_EXTERN v8::Handle<v8::Value> MakeCallback(
    const v8::Handle<v8::Object> object,
    const v8::Handle<v8::String> symbol,
    int argc,
    v8::Handle<v8::Value>* argv);
NODE_EXTERN v8::Handle<v8::Value> MakeCallback(
    const v8::Handle<v8::Object> object,
    const v8::Handle<v8::Function> callback,
    int argc,
    v8::Handle<v8::Value>* argv);

}  // namespace node

#if NODE_WANT_INTERNALS
# include "node_internals.h"
#endif

#ifndef NODE_STRINGIFY
#define NODE_STRINGIFY(n) NODE_STRINGIFY_HELPER(n)
#define NODE_STRINGIFY_HELPER(n) #n
#endif

#ifndef STATIC_ASSERT
#if defined(_MSC_VER)
#  define STATIC_ASSERT(expr) static_assert(expr, "")
# else
#  define STATIC_ASSERT(expr) static_cast<void>((sizeof(char[-1 + !!(expr)])))
# endif
#endif


namespace node {

NODE_EXTERN extern bool no_deprecation;

NODE_EXTERN int Start(int argc, char *argv[]);

char** Init(int argc, char *argv[]);
v8::Handle<v8::Object> SetupProcessObject(int argc, char *argv[]);
void Load(v8::Handle<v8::Object> process);
void EmitExit(v8::Handle<v8::Object> process);

/* Converts a unixtime to V8 Date */
#define NODE_UNIXTIME_V8(t) v8::Date::New(1000*static_cast<double>(t))
#define NODE_V8_UNIXTIME(v) (static_cast<double>((v)->NumberValue())/1000.0);

#define NODE_DEFINE_CONSTANT(target, constant)                            \
  (target)->Set(v8::String::NewSymbol(#constant),                         \
                v8::Number::New(constant),                                \
                static_cast<v8::PropertyAttribute>(                       \
                    v8::ReadOnly|v8::DontDelete))

// Used to be a macro, hence the uppercase name.
template <typename TypeName>
inline void NODE_SET_METHOD(TypeName& recv,
                            const char* name,
                            v8::FunctionCallback callback) {
  v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(callback);
  v8::Local<v8::Function> fn = t->GetFunction();
  v8::Local<v8::String> fn_name = v8::String::New(name);
  fn->SetName(fn_name);
  recv->Set(fn_name, fn);
}
#define NODE_SET_METHOD node::NODE_SET_METHOD

// Used to be a macro, hence the uppercase name.
// Not a template because it only makes sense for FunctionTemplates.
inline void NODE_SET_PROTOTYPE_METHOD(v8::Handle<v8::FunctionTemplate> recv,
                                      const char* name,
                                      v8::FunctionCallback callback) {
  v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(callback);
  recv->PrototypeTemplate()->Set(v8::String::New(name), t->GetFunction());
}
#define NODE_SET_PROTOTYPE_METHOD node::NODE_SET_PROTOTYPE_METHOD

enum encoding {ASCII, UTF8, BASE64, UCS2, BINARY, HEX, BUFFER};
enum encoding ParseEncoding(v8::Handle<v8::Value> encoding_v,
                            enum encoding _default = BINARY);
NODE_EXTERN void FatalException(v8::TryCatch &try_catch);
void DisplayExceptionLine(v8::Handle<v8::Message> message);

NODE_EXTERN v8::Local<v8::Value> Encode(const void *buf, size_t len,
                                        enum encoding encoding = BINARY);

// Returns -1 if the handle was not valid for decoding
NODE_EXTERN ssize_t DecodeBytes(v8::Handle<v8::Value>,
                                enum encoding encoding = BINARY);

// returns bytes written.
NODE_EXTERN ssize_t DecodeWrite(char *buf,
                                size_t buflen,
                                v8::Handle<v8::Value>,
                                enum encoding encoding = BINARY);

v8::Local<v8::Object> BuildStatsObject(const uv_stat_t* s);


#ifdef _WIN32
NODE_EXTERN v8::Local<v8::Value> WinapiErrnoException(int errorno,
    const char *syscall = NULL,  const char *msg = "",
    const char *path = NULL);
#endif

const char *signo_string(int errorno);


NODE_EXTERN typedef void (* addon_register_func)(
    v8::Handle<v8::Object> exports, v8::Handle<v8::Value> module);

struct node_module_struct {
  int version;
  void *dso_handle;
  const char *filename;
  node::addon_register_func register_func;
  const char *modname;
};

node_module_struct* get_builtin_module(const char *name);

/**
 * When this version number is changed, node.js will refuse
 * to load older modules.  This should be done whenever
 * an API is broken in the C++ side, including in v8 or
 * other dependencies.
 */
#define NODE_MODULE_VERSION 0x000C /* v0.12 */

#define NODE_STANDARD_MODULE_STUFF \
          NODE_MODULE_VERSION,     \
          NULL,                    \
          __FILE__

#ifdef _WIN32
# define NODE_MODULE_EXPORT __declspec(dllexport)
#else
# define NODE_MODULE_EXPORT /* empty */
#endif

#define NODE_MODULE(modname, regfunc)                                 \
  extern "C" {                                                        \
    NODE_MODULE_EXPORT node::node_module_struct modname ## _module =  \
    {                                                                 \
      NODE_STANDARD_MODULE_STUFF,                                     \
      (node::addon_register_func)regfunc,                             \
      NODE_STRINGIFY(modname)                                         \
    };                                                                \
  }

#define NODE_MODULE_DECL(modname) \
  extern "C" node::node_module_struct modname ## _module;

/* Called after the event loop exits but before the VM is disposed.
 * Callbacks are run in reverse order of registration, i.e. newest first.
 */
NODE_EXTERN void AtExit(void (*cb)(void* arg), void* arg = 0);

}  // namespace node

#endif  // SRC_NODE_H_

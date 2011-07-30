#include <node.h>
#include <node_buffer.h>
#include <req_wrap.h>
#include <handle_wrap.h>
#include <stream_wrap.h>
#include <pipe_wrap.h>

#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  PipeWrap* wrap =  \
      static_cast<PipeWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) { \
    SetErrno(UV_EBADF); \
    return scope.Close(Integer::New(-1)); \
  }

namespace node {

using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Persistent;
using v8::Value;
using v8::HandleScope;
using v8::FunctionTemplate;
using v8::String;
using v8::Function;
using v8::TryCatch;
using v8::Context;
using v8::Arguments;
using v8::Integer;

Persistent<Function> pipeConstructor;


// TODO share with TCPWrap?
typedef class ReqWrap<uv_connect_t> ConnectWrap;


uv_pipe_t* PipeWrap::UVHandle() {
  return &handle_;
}


PipeWrap* PipeWrap::Unwrap(Local<Object> obj) {
  assert(!obj.IsEmpty());
  assert(obj->InternalFieldCount() > 0);
  return static_cast<PipeWrap*>(obj->GetPointerFromInternalField(0));
}


void PipeWrap::Initialize(Handle<Object> target) {
  StreamWrap::Initialize(target);

  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->SetClassName(String::NewSymbol("Pipe"));

  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "close", HandleWrap::Close);

  NODE_SET_PROTOTYPE_METHOD(t, "readStart", StreamWrap::ReadStart);
  NODE_SET_PROTOTYPE_METHOD(t, "readStop", StreamWrap::ReadStop);
  NODE_SET_PROTOTYPE_METHOD(t, "write", StreamWrap::Write);
  NODE_SET_PROTOTYPE_METHOD(t, "shutdown", StreamWrap::Shutdown);

  NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
  NODE_SET_PROTOTYPE_METHOD(t, "listen", Listen);
  NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);

  pipeConstructor = Persistent<Function>::New(t->GetFunction());

  target->Set(String::NewSymbol("Pipe"), pipeConstructor);
}


Handle<Value> PipeWrap::New(const Arguments& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  assert(args.IsConstructCall());

  HandleScope scope;
  PipeWrap* wrap = new PipeWrap(args.This());
  assert(wrap);

  return scope.Close(args.This());
}


PipeWrap::PipeWrap(Handle<Object> object) : StreamWrap(object,
                                            (uv_stream_t*) &handle_) {
  int r = uv_pipe_init(&handle_);
  assert(r == 0); // How do we proxy this error up to javascript?
                  // Suggestion: uv_pipe_init() returns void.
  handle_.data = reinterpret_cast<void*>(this);
  UpdateWriteQueueSize();
}


Handle<Value> PipeWrap::Bind(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  String::AsciiValue name(args[0]->ToString());

  int r = uv_pipe_bind(&wrap->handle_, *name);

  // Error starting the pipe.
  if (r) SetErrno(uv_last_error().code);

  return scope.Close(Integer::New(r));
}


Handle<Value> PipeWrap::Listen(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  int backlog = args[0]->Int32Value();

  int r = uv_listen((uv_stream_t*)&wrap->handle_, backlog, OnConnection);

  // Error starting the pipe.
  if (r) SetErrno(uv_last_error().code);

  return scope.Close(Integer::New(r));
}


// TODO maybe share with TCPWrap?
void PipeWrap::OnConnection(uv_stream_t* handle, int status) {
  HandleScope scope;

  PipeWrap* wrap = static_cast<PipeWrap*>(handle->data);
  assert(&wrap->handle_ == (uv_pipe_t*)handle);

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  assert(wrap->object_.IsEmpty() == false);

  if (status != 0) {
    // TODO Handle server error (set errno and call onconnection with NULL)
    assert(0);
    return;
  }

  // Instanciate the client javascript object and handle.
  Local<Object> client_obj = pipeConstructor->NewInstance();

  // Unwrap the client javascript object.
  assert(client_obj->InternalFieldCount() > 0);
  PipeWrap* client_wrap =
      static_cast<PipeWrap*>(client_obj->GetPointerFromInternalField(0));

  int r = uv_accept(handle, (uv_stream_t*)&client_wrap->handle_);

  // uv_accept should always work.
  assert(r == 0);

  // Successful accept. Call the onconnection callback in JavaScript land.
  Local<Value> argv[1] = { client_obj };
  MakeCallback(wrap->object_, "onconnection", 1, argv);
}

// TODO Maybe share this with TCPWrap?
void PipeWrap::AfterConnect(uv_connect_t* req, int status) {
  ConnectWrap* req_wrap = (ConnectWrap*) req->data;
  PipeWrap* wrap = (PipeWrap*) req->handle->data;

  HandleScope scope;

  // The wrap and request objects should still be there.
  assert(req_wrap->object_.IsEmpty() == false);
  assert(wrap->object_.IsEmpty() == false);

  if (status) {
    SetErrno(uv_last_error().code);
  }

  Local<Value> argv[3] = {
    Integer::New(status),
    Local<Value>::New(wrap->object_),
    Local<Value>::New(req_wrap->object_)
  };

  MakeCallback(req_wrap->object_, "oncomplete", 3, argv);

  delete req_wrap;
}


Handle<Value> PipeWrap::Connect(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  String::AsciiValue name(args[0]->ToString());

  ConnectWrap* req_wrap = new ConnectWrap();

  int r = uv_pipe_connect(&req_wrap->req_,
                          &wrap->handle_,
                          *name,
                          AfterConnect);

  req_wrap->Dispatched();

  if (r) {
    SetErrno(uv_last_error().code);
    delete req_wrap;
    return scope.Close(v8::Null());
  } else {
    return scope.Close(req_wrap->object_);
  }
}


}  // namespace node

NODE_MODULE(node_pipe_wrap, node::PipeWrap::Initialize);

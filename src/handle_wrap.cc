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

#include "handle_wrap.h"
#include "node.h"
#include "queue.h"

namespace node {

using v8::FunctionCallbackInfo;
using v8::Handle;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

// defined in node.cc
extern QUEUE handle_wrap_queue;
static Cached<String> close_sym;


void HandleWrap::Ref(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  HandleWrap* wrap;
  NODE_UNWRAP_NO_ABORT(args.This(), HandleWrap, wrap);

  if (wrap != NULL && wrap->handle__ != NULL) {
    uv_ref(wrap->handle__);
    wrap->flags_ &= ~kUnref;
  }
}


void HandleWrap::Unref(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  HandleWrap* wrap;
  NODE_UNWRAP_NO_ABORT(args.This(), HandleWrap, wrap);

  if (wrap != NULL && wrap->handle__ != NULL) {
    uv_unref(wrap->handle__);
    wrap->flags_ |= kUnref;
  }
}


void HandleWrap::Close(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  HandleWrap* wrap;
  NODE_UNWRAP(args.This(), HandleWrap, wrap);

  // guard against uninitialized handle or double close
  if (wrap == NULL || wrap->handle__ == NULL) return;

  assert(!wrap->persistent().IsEmpty());
  uv_close(wrap->handle__, OnClose);
  wrap->handle__ = NULL;

  if (args[0]->IsFunction()) {
    if (close_sym.IsEmpty() == true) {
      close_sym = FIXED_ONE_BYTE_STRING(node_isolate, "close");
    }
    wrap->object()->Set(close_sym, args[0]);
    wrap->flags_ |= kCloseCallback;
  }
}


HandleWrap::HandleWrap(Handle<Object> object, uv_handle_t* h) {
  flags_ = 0;
  handle__ = h;
  handle__->data = this;

  HandleScope scope(node_isolate);
  assert(persistent().IsEmpty());
  persistent().Reset(node_isolate, object);
  NODE_WRAP(object, this);
  QUEUE_INSERT_TAIL(&handle_wrap_queue, &handle_wrap_queue_);
}


HandleWrap::~HandleWrap() {
  assert(persistent().IsEmpty());
  QUEUE_REMOVE(&handle_wrap_queue_);
}


void HandleWrap::OnClose(uv_handle_t* handle) {
  HandleWrap* wrap = static_cast<HandleWrap*>(handle->data);

  // The wrap object should still be there.
  assert(wrap->persistent().IsEmpty() == false);

  // But the handle pointer should be gone.
  assert(wrap->handle__ == NULL);

  HandleScope scope(node_isolate);
  Local<Object> object = wrap->object();

  if (wrap->flags_ & kCloseCallback) {
    assert(close_sym.IsEmpty() == false);
    MakeCallback(object, close_sym, 0, NULL);
  }

  object->SetAlignedPointerInInternalField(0, NULL);
  wrap->persistent().Dispose();
  delete wrap;
}


}  // namespace node

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

#ifndef SRC_WEAK_OBJECT_H_
#define SRC_WEAK_OBJECT_H_

#include "v8.h"

namespace node {

class WeakObject {
 public:
  // FIXME(bnoordhuis) These methods are public only because the code base
  // plays fast and loose with encapsulation.
  template <typename TypeName>
  inline static TypeName* Unwrap(v8::Local<v8::Object> object);
  // Returns the wrapped object.  Illegal to call in your destructor.
  inline v8::Local<v8::Object> weak_object(v8::Isolate* isolate) const;
 protected:
  // |object| should be an instance of a v8::ObjectTemplate that has at least
  // one internal field reserved with v8::ObjectTemplate::SetInternalFieldCount.
  inline WeakObject(v8::Isolate* isolate, v8::Local<v8::Object> object);
  virtual inline ~WeakObject();
  inline void MakeWeak();
  inline void ClearWeak();
 private:
  inline static void WeakCallback(v8::Isolate* isolate,
                                  v8::Persistent<v8::Object>* persistent,
                                  WeakObject* self);
  static const int kInternalFieldIndex = 0;
  v8::Persistent<v8::Object> weak_object_;
};

}  // namespace node

#endif  // SRC_WEAK_OBJECT_H_

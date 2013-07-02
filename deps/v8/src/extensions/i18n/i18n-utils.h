// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// limitations under the License.

#ifndef V8_EXTENSIONS_I18N_SRC_UTILS_H_
#define V8_EXTENSIONS_I18N_SRC_UTILS_H_

#include "unicode/uversion.h"
#include "v8.h"

namespace U_ICU_NAMESPACE {
class UnicodeString;
}

namespace v8_i18n {

class Utils {
 public:
  // Safe string copy. Null terminates the destination. Copies at most
  // (length - 1) bytes.
  // We can't use snprintf since it's not supported on all relevant platforms.
  // We can't use OS::SNPrintF, it's only for internal code.
  static void StrNCopy(char* dest, int length, const char* src);

  // Converts v8::String into UnicodeString. Returns false if input
  // can't be converted into utf8.
  static bool V8StringToUnicodeString(const v8::Handle<v8::Value>& input,
                                      icu::UnicodeString* output);

  // Extract a String setting named in |settings| and set it to |result|.
  // Return true if it's specified. Otherwise, return false.
  static bool ExtractStringSetting(const v8::Handle<v8::Object>& settings,
                                   const char* setting,
                                   icu::UnicodeString* result);

  // Extract a Integer setting named in |settings| and set it to |result|.
  // Return true if it's specified. Otherwise, return false.
  static bool ExtractIntegerSetting(const v8::Handle<v8::Object>& settings,
                                    const char* setting,
                                    int32_t* result);

  // Extract a Boolean setting named in |settings| and set it to |result|.
  // Return true if it's specified. Otherwise, return false.
  static bool ExtractBooleanSetting(const v8::Handle<v8::Object>& settings,
                                    const char* setting,
                                    bool* result);

  // Converts ASCII array into UChar array.
  // Target is always \0 terminated.
  static void AsciiToUChar(const char* source,
                           int32_t source_length,
                           UChar* target,
                           int32_t target_length);

  // Creates an ObjectTemplate with one internal field.
  static v8::Local<v8::ObjectTemplate> GetTemplate(v8::Isolate* isolate);

  // Creates an ObjectTemplate with two internal fields.
  static v8::Local<v8::ObjectTemplate> GetTemplate2(v8::Isolate* isolate);

 private:
  Utils() {}
};

}  // namespace v8_i18n

#endif  // V8_EXTENSIONS_I18N_UTILS_H_

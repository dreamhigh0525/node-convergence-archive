// Copyright 2011 the V8 project authors. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "v8.h"

#include "cctest.h"
#include "execution.h"
#include "isolate.h"
#include "parser.h"
#include "preparser.h"
#include "scanner-character-streams.h"
#include "token.h"
#include "utils.h"

TEST(ScanKeywords) {
  struct KeywordToken {
    const char* keyword;
    i::Token::Value token;
  };

  static const KeywordToken keywords[] = {
#define KEYWORD(t, s, d) { s, i::Token::t },
      TOKEN_LIST(IGNORE_TOKEN, KEYWORD)
#undef KEYWORD
      { NULL, i::Token::IDENTIFIER }
  };

  KeywordToken key_token;
  i::UnicodeCache unicode_cache;
  i::byte buffer[32];
  for (int i = 0; (key_token = keywords[i]).keyword != NULL; i++) {
    const i::byte* keyword =
        reinterpret_cast<const i::byte*>(key_token.keyword);
    int length = i::StrLength(key_token.keyword);
    CHECK(static_cast<int>(sizeof(buffer)) >= length);
    {
      i::Utf8ToUC16CharacterStream stream(keyword, length);
      i::JavaScriptScanner scanner(&unicode_cache);
      // The scanner should parse 'let' as Token::LET for this test.
      scanner.SetHarmonyBlockScoping(true);
      scanner.Initialize(&stream);
      CHECK_EQ(key_token.token, scanner.Next());
      CHECK_EQ(i::Token::EOS, scanner.Next());
    }
    // Removing characters will make keyword matching fail.
    {
      i::Utf8ToUC16CharacterStream stream(keyword, length - 1);
      i::JavaScriptScanner scanner(&unicode_cache);
      scanner.Initialize(&stream);
      CHECK_EQ(i::Token::IDENTIFIER, scanner.Next());
      CHECK_EQ(i::Token::EOS, scanner.Next());
    }
    // Adding characters will make keyword matching fail.
    static const char chars_to_append[] = { 'z', '0', '_' };
    for (int j = 0; j < static_cast<int>(ARRAY_SIZE(chars_to_append)); ++j) {
      memmove(buffer, keyword, length);
      buffer[length] = chars_to_append[j];
      i::Utf8ToUC16CharacterStream stream(buffer, length + 1);
      i::JavaScriptScanner scanner(&unicode_cache);
      scanner.Initialize(&stream);
      CHECK_EQ(i::Token::IDENTIFIER, scanner.Next());
      CHECK_EQ(i::Token::EOS, scanner.Next());
    }
    // Replacing characters will make keyword matching fail.
    {
      memmove(buffer, keyword, length);
      buffer[length - 1] = '_';
      i::Utf8ToUC16CharacterStream stream(buffer, length);
      i::JavaScriptScanner scanner(&unicode_cache);
      scanner.Initialize(&stream);
      CHECK_EQ(i::Token::IDENTIFIER, scanner.Next());
      CHECK_EQ(i::Token::EOS, scanner.Next());
    }
  }
}


TEST(ScanHTMLEndComments) {
  v8::V8::Initialize();

  // Regression test. See:
  //    http://code.google.com/p/chromium/issues/detail?id=53548
  // Tests that --> is correctly interpreted as comment-to-end-of-line if there
  // is only whitespace before it on the line (with comments considered as
  // whitespace, even a multiline-comment containing a newline).
  // This was not the case if it occurred before the first real token
  // in the input.
  const char* tests[] = {
      // Before first real token.
      "--> is eol-comment\nvar y = 37;\n",
      "\n --> is eol-comment\nvar y = 37;\n",
      "/* precomment */ --> is eol-comment\nvar y = 37;\n",
      "\n/* precomment */ --> is eol-comment\nvar y = 37;\n",
      // After first real token.
      "var x = 42;\n--> is eol-comment\nvar y = 37;\n",
      "var x = 42;\n/* precomment */ --> is eol-comment\nvar y = 37;\n",
      NULL
  };

  const char* fail_tests[] = {
      "x --> is eol-comment\nvar y = 37;\n",
      "\"\\n\" --> is eol-comment\nvar y = 37;\n",
      "x/* precomment */ --> is eol-comment\nvar y = 37;\n",
      "x/* precomment\n */ --> is eol-comment\nvar y = 37;\n",
      "var x = 42; --> is eol-comment\nvar y = 37;\n",
      "var x = 42; /* precomment\n */ --> is eol-comment\nvar y = 37;\n",
      NULL
  };

  // Parser/Scanner needs a stack limit.
  int marker;
  i::Isolate::Current()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  for (int i = 0; tests[i]; i++) {
    v8::ScriptData* data =
        v8::ScriptData::PreCompile(tests[i], i::StrLength(tests[i]));
    CHECK(data != NULL && !data->HasError());
    delete data;
  }

  for (int i = 0; fail_tests[i]; i++) {
    v8::ScriptData* data =
        v8::ScriptData::PreCompile(fail_tests[i], i::StrLength(fail_tests[i]));
    CHECK(data == NULL || data->HasError());
    delete data;
  }
}


class ScriptResource : public v8::String::ExternalAsciiStringResource {
 public:
  ScriptResource(const char* data, size_t length)
      : data_(data), length_(length) { }

  const char* data() const { return data_; }
  size_t length() const { return length_; }

 private:
  const char* data_;
  size_t length_;
};


TEST(Preparsing) {
  v8::HandleScope handles;
  v8::Persistent<v8::Context> context = v8::Context::New();
  v8::Context::Scope context_scope(context);
  int marker;
  i::Isolate::Current()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  // Source containing functions that might be lazily compiled  and all types
  // of symbols (string, propertyName, regexp).
  const char* source =
      "var x = 42;"
      "function foo(a) { return function nolazy(b) { return a + b; } }"
      "function bar(a) { if (a) return function lazy(b) { return b; } }"
      "var z = {'string': 'string literal', bareword: 'propertyName', "
      "         42: 'number literal', for: 'keyword as propertyName', "
      "         f\\u006fr: 'keyword propertyname with escape'};"
      "var v = /RegExp Literal/;"
      "var w = /RegExp Literal\\u0020With Escape/gin;"
      "var y = { get getter() { return 42; }, "
      "          set setter(v) { this.value = v; }};";
  int source_length = i::StrLength(source);
  const char* error_source = "var x = y z;";
  int error_source_length = i::StrLength(error_source);

  v8::ScriptData* preparse =
      v8::ScriptData::PreCompile(source, source_length);
  CHECK(!preparse->HasError());
  bool lazy_flag = i::FLAG_lazy;
  {
    i::FLAG_lazy = true;
    ScriptResource* resource = new ScriptResource(source, source_length);
    v8::Local<v8::String> script_source = v8::String::NewExternal(resource);
    v8::Script::Compile(script_source, NULL, preparse);
  }

  {
    i::FLAG_lazy = false;

    ScriptResource* resource = new ScriptResource(source, source_length);
    v8::Local<v8::String> script_source = v8::String::NewExternal(resource);
    v8::Script::New(script_source, NULL, preparse, v8::Local<v8::String>());
  }
  delete preparse;
  i::FLAG_lazy = lazy_flag;

  // Syntax error.
  v8::ScriptData* error_preparse =
      v8::ScriptData::PreCompile(error_source, error_source_length);
  CHECK(error_preparse->HasError());
  i::ScriptDataImpl *pre_impl =
      reinterpret_cast<i::ScriptDataImpl*>(error_preparse);
  i::Scanner::Location error_location =
      pre_impl->MessageLocation();
  // Error is at "z" in source, location 10..11.
  CHECK_EQ(10, error_location.beg_pos);
  CHECK_EQ(11, error_location.end_pos);
  // Should not crash.
  const char* message = pre_impl->BuildMessage();
  i::Vector<const char*> args = pre_impl->BuildArgs();
  CHECK_GT(strlen(message), 0);
}


TEST(StandAlonePreParser) {
  v8::V8::Initialize();

  int marker;
  i::Isolate::Current()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* programs[] = {
      "{label: 42}",
      "var x = 42;",
      "function foo(x, y) { return x + y; }",
      "%ArgleBargle(glop);",
      "var x = new new Function('this.x = 42');",
      NULL
  };

  uintptr_t stack_limit = i::Isolate::Current()->stack_guard()->real_climit();
  for (int i = 0; programs[i]; i++) {
    const char* program = programs[i];
    i::Utf8ToUC16CharacterStream stream(
        reinterpret_cast<const i::byte*>(program),
        static_cast<unsigned>(strlen(program)));
    i::CompleteParserRecorder log;
    i::JavaScriptScanner scanner(i::Isolate::Current()->unicode_cache());
    scanner.Initialize(&stream);

    v8::preparser::PreParser::PreParseResult result =
        v8::preparser::PreParser::PreParseProgram(&scanner,
                                                  &log,
                                                  true,
                                                  stack_limit);
    CHECK_EQ(v8::preparser::PreParser::kPreParseSuccess, result);
    i::ScriptDataImpl data(log.ExtractData());
    CHECK(!data.has_error());
  }
}


TEST(RegressChromium62639) {
  v8::V8::Initialize();

  int marker;
  i::Isolate::Current()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* program = "var x = 'something';\n"
                        "escape: function() {}";
  // Fails parsing expecting an identifier after "function".
  // Before fix, didn't check *ok after Expect(Token::Identifier, ok),
  // and then used the invalid currently scanned literal. This always
  // failed in debug mode, and sometimes crashed in release mode.

  i::Utf8ToUC16CharacterStream stream(reinterpret_cast<const i::byte*>(program),
                                      static_cast<unsigned>(strlen(program)));
  i::ScriptDataImpl* data =
      i::ParserApi::PreParse(&stream, NULL, false);
  CHECK(data->HasError());
  delete data;
}


TEST(Regress928) {
  v8::V8::Initialize();

  // Preparsing didn't consider the catch clause of a try statement
  // as with-content, which made it assume that a function inside
  // the block could be lazily compiled, and an extra, unexpected,
  // entry was added to the data.
  int marker;
  i::Isolate::Current()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  const char* program =
      "try { } catch (e) { var foo = function () { /* first */ } }"
      "var bar = function () { /* second */ }";

  i::Utf8ToUC16CharacterStream stream(reinterpret_cast<const i::byte*>(program),
                                      static_cast<unsigned>(strlen(program)));
  i::ScriptDataImpl* data =
      i::ParserApi::PartialPreParse(&stream, NULL, false);
  CHECK(!data->HasError());

  data->Initialize();

  int first_function =
      static_cast<int>(strstr(program, "function") - program);
  int first_lbrace = first_function + static_cast<int>(strlen("function () "));
  CHECK_EQ('{', program[first_lbrace]);
  i::FunctionEntry entry1 = data->GetFunctionEntry(first_lbrace);
  CHECK(!entry1.is_valid());

  int second_function =
      static_cast<int>(strstr(program + first_lbrace, "function") - program);
  int second_lbrace =
      second_function + static_cast<int>(strlen("function () "));
  CHECK_EQ('{', program[second_lbrace]);
  i::FunctionEntry entry2 = data->GetFunctionEntry(second_lbrace);
  CHECK(entry2.is_valid());
  CHECK_EQ('}', program[entry2.end_pos() - 1]);
  delete data;
}


TEST(PreParseOverflow) {
  v8::V8::Initialize();

  int marker;
  i::Isolate::Current()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  size_t kProgramSize = 1024 * 1024;
  i::SmartArrayPointer<char> program(
      reinterpret_cast<char*>(malloc(kProgramSize + 1)));
  memset(*program, '(', kProgramSize);
  program[kProgramSize] = '\0';

  uintptr_t stack_limit = i::Isolate::Current()->stack_guard()->real_climit();

  i::Utf8ToUC16CharacterStream stream(
      reinterpret_cast<const i::byte*>(*program),
      static_cast<unsigned>(kProgramSize));
  i::CompleteParserRecorder log;
  i::JavaScriptScanner scanner(i::Isolate::Current()->unicode_cache());
  scanner.Initialize(&stream);


  v8::preparser::PreParser::PreParseResult result =
      v8::preparser::PreParser::PreParseProgram(&scanner,
                                                &log,
                                                true,
                                                stack_limit);
  CHECK_EQ(v8::preparser::PreParser::kPreParseStackOverflow, result);
}


class TestExternalResource: public v8::String::ExternalStringResource {
 public:
  explicit TestExternalResource(uint16_t* data, int length)
      : data_(data), length_(static_cast<size_t>(length)) { }

  ~TestExternalResource() { }

  const uint16_t* data() const {
    return data_;
  }

  size_t length() const {
    return length_;
  }
 private:
  uint16_t* data_;
  size_t length_;
};


#define CHECK_EQU(v1, v2) CHECK_EQ(static_cast<int>(v1), static_cast<int>(v2))

void TestCharacterStream(const char* ascii_source,
                         unsigned length,
                         unsigned start = 0,
                         unsigned end = 0) {
  if (end == 0) end = length;
  unsigned sub_length = end - start;
  i::HandleScope test_scope;
  i::SmartArrayPointer<i::uc16> uc16_buffer(new i::uc16[length]);
  for (unsigned i = 0; i < length; i++) {
    uc16_buffer[i] = static_cast<i::uc16>(ascii_source[i]);
  }
  i::Vector<const char> ascii_vector(ascii_source, static_cast<int>(length));
  i::Handle<i::String> ascii_string(
      FACTORY->NewStringFromAscii(ascii_vector));
  TestExternalResource resource(*uc16_buffer, length);
  i::Handle<i::String> uc16_string(
      FACTORY->NewExternalStringFromTwoByte(&resource));

  i::ExternalTwoByteStringUC16CharacterStream uc16_stream(
      i::Handle<i::ExternalTwoByteString>::cast(uc16_string), start, end);
  i::GenericStringUC16CharacterStream string_stream(ascii_string, start, end);
  i::Utf8ToUC16CharacterStream utf8_stream(
      reinterpret_cast<const i::byte*>(ascii_source), end);
  utf8_stream.SeekForward(start);

  unsigned i = start;
  while (i < end) {
    // Read streams one char at a time
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    int32_t c0 = ascii_source[i];
    int32_t c1 = uc16_stream.Advance();
    int32_t c2 = string_stream.Advance();
    int32_t c3 = utf8_stream.Advance();
    i++;
    CHECK_EQ(c0, c1);
    CHECK_EQ(c0, c2);
    CHECK_EQ(c0, c3);
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
  }
  while (i > start + sub_length / 4) {
    // Pushback, re-read, pushback again.
    int32_t c0 = ascii_source[i - 1];
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    uc16_stream.PushBack(c0);
    string_stream.PushBack(c0);
    utf8_stream.PushBack(c0);
    i--;
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    int32_t c1 = uc16_stream.Advance();
    int32_t c2 = string_stream.Advance();
    int32_t c3 = utf8_stream.Advance();
    i++;
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    CHECK_EQ(c0, c1);
    CHECK_EQ(c0, c2);
    CHECK_EQ(c0, c3);
    uc16_stream.PushBack(c0);
    string_stream.PushBack(c0);
    utf8_stream.PushBack(c0);
    i--;
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
  }
  unsigned halfway = start + sub_length / 2;
  uc16_stream.SeekForward(halfway - i);
  string_stream.SeekForward(halfway - i);
  utf8_stream.SeekForward(halfway - i);
  i = halfway;
  CHECK_EQU(i, uc16_stream.pos());
  CHECK_EQU(i, string_stream.pos());
  CHECK_EQU(i, utf8_stream.pos());

  while (i < end) {
    // Read streams one char at a time
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
    int32_t c0 = ascii_source[i];
    int32_t c1 = uc16_stream.Advance();
    int32_t c2 = string_stream.Advance();
    int32_t c3 = utf8_stream.Advance();
    i++;
    CHECK_EQ(c0, c1);
    CHECK_EQ(c0, c2);
    CHECK_EQ(c0, c3);
    CHECK_EQU(i, uc16_stream.pos());
    CHECK_EQU(i, string_stream.pos());
    CHECK_EQU(i, utf8_stream.pos());
  }

  int32_t c1 = uc16_stream.Advance();
  int32_t c2 = string_stream.Advance();
  int32_t c3 = utf8_stream.Advance();
  CHECK_LT(c1, 0);
  CHECK_LT(c2, 0);
  CHECK_LT(c3, 0);
}


TEST(CharacterStreams) {
  v8::HandleScope handles;
  v8::Persistent<v8::Context> context = v8::Context::New();
  v8::Context::Scope context_scope(context);

  TestCharacterStream("abc\0\n\r\x7f", 7);
  static const unsigned kBigStringSize = 4096;
  char buffer[kBigStringSize + 1];
  for (unsigned i = 0; i < kBigStringSize; i++) {
    buffer[i] = static_cast<char>(i & 0x7f);
  }
  TestCharacterStream(buffer, kBigStringSize);

  TestCharacterStream(buffer, kBigStringSize, 576, 3298);

  TestCharacterStream("\0", 1);
  TestCharacterStream("", 0);
}


TEST(Utf8CharacterStream) {
  static const unsigned kMaxUC16CharU = unibrow::Utf8::kMaxThreeByteChar;
  static const int kMaxUC16Char = static_cast<int>(kMaxUC16CharU);

  static const int kAllUtf8CharsSize =
      (unibrow::Utf8::kMaxOneByteChar + 1) +
      (unibrow::Utf8::kMaxTwoByteChar - unibrow::Utf8::kMaxOneByteChar) * 2 +
      (unibrow::Utf8::kMaxThreeByteChar - unibrow::Utf8::kMaxTwoByteChar) * 3;
  static const unsigned kAllUtf8CharsSizeU =
      static_cast<unsigned>(kAllUtf8CharsSize);

  char buffer[kAllUtf8CharsSizeU];
  unsigned cursor = 0;
  for (int i = 0; i <= kMaxUC16Char; i++) {
    cursor += unibrow::Utf8::Encode(buffer + cursor, i);
  }
  ASSERT(cursor == kAllUtf8CharsSizeU);

  i::Utf8ToUC16CharacterStream stream(reinterpret_cast<const i::byte*>(buffer),
                                      kAllUtf8CharsSizeU);
  for (int i = 0; i <= kMaxUC16Char; i++) {
    CHECK_EQU(i, stream.pos());
    int32_t c = stream.Advance();
    CHECK_EQ(i, c);
    CHECK_EQU(i + 1, stream.pos());
  }
  for (int i = kMaxUC16Char; i >= 0; i--) {
    CHECK_EQU(i + 1, stream.pos());
    stream.PushBack(i);
    CHECK_EQU(i, stream.pos());
  }
  int i = 0;
  while (stream.pos() < kMaxUC16CharU) {
    CHECK_EQU(i, stream.pos());
    unsigned progress = stream.SeekForward(12);
    i += progress;
    int32_t c = stream.Advance();
    if (i <= kMaxUC16Char) {
      CHECK_EQ(i, c);
    } else {
      CHECK_EQ(-1, c);
    }
    i += 1;
    CHECK_EQU(i, stream.pos());
  }
}

#undef CHECK_EQU

void TestStreamScanner(i::UC16CharacterStream* stream,
                       i::Token::Value* expected_tokens,
                       int skip_pos = 0,  // Zero means not skipping.
                       int skip_to = 0) {
  i::JavaScriptScanner scanner(i::Isolate::Current()->unicode_cache());
  scanner.Initialize(stream);

  int i = 0;
  do {
    i::Token::Value expected = expected_tokens[i];
    i::Token::Value actual = scanner.Next();
    CHECK_EQ(i::Token::String(expected), i::Token::String(actual));
    if (scanner.location().end_pos == skip_pos) {
      scanner.SeekForward(skip_to);
    }
    i++;
  } while (expected_tokens[i] != i::Token::ILLEGAL);
}

TEST(StreamScanner) {
  v8::V8::Initialize();

  const char* str1 = "{ foo get for : */ <- \n\n /*foo*/ bib";
  i::Utf8ToUC16CharacterStream stream1(reinterpret_cast<const i::byte*>(str1),
                                       static_cast<unsigned>(strlen(str1)));
  i::Token::Value expectations1[] = {
      i::Token::LBRACE,
      i::Token::IDENTIFIER,
      i::Token::IDENTIFIER,
      i::Token::FOR,
      i::Token::COLON,
      i::Token::MUL,
      i::Token::DIV,
      i::Token::LT,
      i::Token::SUB,
      i::Token::IDENTIFIER,
      i::Token::EOS,
      i::Token::ILLEGAL
  };
  TestStreamScanner(&stream1, expectations1, 0, 0);

  const char* str2 = "case default const {THIS\nPART\nSKIPPED} do";
  i::Utf8ToUC16CharacterStream stream2(reinterpret_cast<const i::byte*>(str2),
                                       static_cast<unsigned>(strlen(str2)));
  i::Token::Value expectations2[] = {
      i::Token::CASE,
      i::Token::DEFAULT,
      i::Token::CONST,
      i::Token::LBRACE,
      // Skipped part here
      i::Token::RBRACE,
      i::Token::DO,
      i::Token::EOS,
      i::Token::ILLEGAL
  };
  ASSERT_EQ('{', str2[19]);
  ASSERT_EQ('}', str2[37]);
  TestStreamScanner(&stream2, expectations2, 20, 37);

  const char* str3 = "{}}}}";
  i::Token::Value expectations3[] = {
      i::Token::LBRACE,
      i::Token::RBRACE,
      i::Token::RBRACE,
      i::Token::RBRACE,
      i::Token::RBRACE,
      i::Token::EOS,
      i::Token::ILLEGAL
  };
  // Skip zero-four RBRACEs.
  for (int i = 0; i <= 4; i++) {
     expectations3[6 - i] = i::Token::ILLEGAL;
     expectations3[5 - i] = i::Token::EOS;
     i::Utf8ToUC16CharacterStream stream3(
         reinterpret_cast<const i::byte*>(str3),
         static_cast<unsigned>(strlen(str3)));
     TestStreamScanner(&stream3, expectations3, 1, 1 + i);
  }
}


void TestScanRegExp(const char* re_source, const char* expected) {
  i::Utf8ToUC16CharacterStream stream(
       reinterpret_cast<const i::byte*>(re_source),
       static_cast<unsigned>(strlen(re_source)));
  i::JavaScriptScanner scanner(i::Isolate::Current()->unicode_cache());
  scanner.Initialize(&stream);

  i::Token::Value start = scanner.peek();
  CHECK(start == i::Token::DIV || start == i::Token::ASSIGN_DIV);
  CHECK(scanner.ScanRegExpPattern(start == i::Token::ASSIGN_DIV));
  scanner.Next();  // Current token is now the regexp literal.
  CHECK(scanner.is_literal_ascii());
  i::Vector<const char> actual = scanner.literal_ascii_string();
  for (int i = 0; i < actual.length(); i++) {
    CHECK_NE('\0', expected[i]);
    CHECK_EQ(expected[i], actual[i]);
  }
}


TEST(RegExpScanning) {
  v8::V8::Initialize();

  // RegExp token with added garbage at the end. The scanner should only
  // scan the RegExp until the terminating slash just before "flipperwald".
  TestScanRegExp("/b/flipperwald", "b");
  // Incomplete escape sequences doesn't hide the terminating slash.
  TestScanRegExp("/\\x/flipperwald", "\\x");
  TestScanRegExp("/\\u/flipperwald", "\\u");
  TestScanRegExp("/\\u1/flipperwald", "\\u1");
  TestScanRegExp("/\\u12/flipperwald", "\\u12");
  TestScanRegExp("/\\u123/flipperwald", "\\u123");
  TestScanRegExp("/\\c/flipperwald", "\\c");
  TestScanRegExp("/\\c//flipperwald", "\\c");
  // Slashes inside character classes are not terminating.
  TestScanRegExp("/[/]/flipperwald", "[/]");
  TestScanRegExp("/[\\s-/]/flipperwald", "[\\s-/]");
  // Incomplete escape sequences inside a character class doesn't hide
  // the end of the character class.
  TestScanRegExp("/[\\c/]/flipperwald", "[\\c/]");
  TestScanRegExp("/[\\c]/flipperwald", "[\\c]");
  TestScanRegExp("/[\\x]/flipperwald", "[\\x]");
  TestScanRegExp("/[\\x1]/flipperwald", "[\\x1]");
  TestScanRegExp("/[\\u]/flipperwald", "[\\u]");
  TestScanRegExp("/[\\u1]/flipperwald", "[\\u1]");
  TestScanRegExp("/[\\u12]/flipperwald", "[\\u12]");
  TestScanRegExp("/[\\u123]/flipperwald", "[\\u123]");
  // Escaped ']'s wont end the character class.
  TestScanRegExp("/[\\]/]/flipperwald", "[\\]/]");
  // Escaped slashes are not terminating.
  TestScanRegExp("/\\//flipperwald", "\\/");
  // Starting with '=' works too.
  TestScanRegExp("/=/", "=");
  TestScanRegExp("/=?/", "=?");
}


void TestParserSync(i::Handle<i::String> source, bool allow_lazy) {
  uintptr_t stack_limit = i::Isolate::Current()->stack_guard()->real_climit();

  // Preparse the data.
  i::CompleteParserRecorder log;
  i::JavaScriptScanner scanner(i::Isolate::Current()->unicode_cache());
  i::GenericStringUC16CharacterStream stream(source, 0, source->length());
  scanner.Initialize(&stream);
  v8::preparser::PreParser::PreParseResult result =
      v8::preparser::PreParser::PreParseProgram(
          &scanner, &log, allow_lazy, stack_limit);
  CHECK_EQ(v8::preparser::PreParser::kPreParseSuccess, result);
  i::ScriptDataImpl data(log.ExtractData());

  // Parse the data
  i::Handle<i::Script> script = FACTORY->NewScript(source);
  i::Parser parser(script, false, NULL, NULL);
  i::FunctionLiteral* function =
      parser.ParseProgram(source, true, i::kNonStrictMode);

  i::String* type_string = NULL;
  if (function == NULL) {
    // Extract exception from the parser.
    i::Handle<i::String> type_symbol = FACTORY->LookupAsciiSymbol("type");
    CHECK(i::Isolate::Current()->has_pending_exception());
    i::MaybeObject* maybe_object = i::Isolate::Current()->pending_exception();
    i::JSObject* exception = NULL;
    CHECK(maybe_object->To(&exception));

    // Get the type string.
    maybe_object = exception->GetProperty(*type_symbol);
    CHECK(maybe_object->To(&type_string));
  }

  // Check that preparsing fails iff parsing fails.
  if (data.has_error() && function != NULL) {
    i::OS::Print(
        "Preparser failed on:\n"
        "\t%s\n"
        "with error:\n"
        "\t%s\n"
        "However, the parser succeeded",
        *source->ToCString(), data.BuildMessage());
    CHECK(false);
  } else if (!data.has_error() && function == NULL) {
    i::OS::Print(
        "Parser failed on:\n"
        "\t%s\n"
        "with error:\n"
        "\t%s\n"
        "However, the preparser succeeded",
        *source->ToCString(), *type_string->ToCString());
    CHECK(false);
  }

  // Check that preparser and parser produce the same error.
  if (function == NULL) {
    if (!type_string->IsEqualTo(i::CStrVector(data.BuildMessage()))) {
      i::OS::Print(
          "Expected parser and preparser to produce the same error on:\n"
          "\t%s\n"
          "However, found the following error messages\n"
          "\tparser:    %s\n"
          "\tpreparser: %s\n",
          *source->ToCString(), *type_string->ToCString(), data.BuildMessage());
      CHECK(false);
    }
  }
}


TEST(ParserSync) {
  const char* context_data[][2] = {
    { "", "" },
    { "{", "}" },
    { "if (true) ", " else {}" },
    { "if (true) {} else ", "" },
    { "if (true) ", "" },
    { "do ", " while (false)" },
    { "while (false) ", "" },
    { "for (;;) ", "" },
    { "with ({})", "" },
    { "switch (12) { case 12: ", "}" },
    { "switch (12) { default: ", "}" },
    { "label2: ", "" },
    { NULL, NULL }
  };

  const char* statement_data[] = {
    "{}",
    "var x",
    "var x = 1",
    "const x",
    "const x = 1",
    ";",
    "12",
    "if (false) {} else ;",
    "if (false) {} else {}",
    "if (false) {} else 12",
    "if (false) ;"
    "if (false) {}",
    "if (false) 12",
    "do {} while (false)",
    "for (;;) ;",
    "for (;;) {}",
    "for (;;) 12",
    "continue",
    "continue label",
    "continue\nlabel",
    "break",
    "break label",
    "break\nlabel",
    "return",
    "return  12",
    "return\n12",
    "with ({}) ;",
    "with ({}) {}",
    "with ({}) 12",
    "switch ({}) { default: }"
    "label3: "
    "throw",
    "throw  12",
    "throw\n12",
    "try {} catch(e) {}",
    "try {} finally {}",
    "try {} catch(e) {} finally {}",
    "debugger",
    NULL
  };

  const char* termination_data[] = {
    "",
    ";",
    "\n",
    ";\n",
    "\n;",
    NULL
  };

  v8::HandleScope handles;
  v8::Persistent<v8::Context> context = v8::Context::New();
  v8::Context::Scope context_scope(context);

  int marker;
  i::Isolate::Current()->stack_guard()->SetStackLimit(
      reinterpret_cast<uintptr_t>(&marker) - 128 * 1024);

  for (int i = 0; context_data[i][0] != NULL; ++i) {
    for (int j = 0; statement_data[j] != NULL; ++j) {
      for (int k = 0; termination_data[k] != NULL; ++k) {
        int kPrefixLen = i::StrLength(context_data[i][0]);
        int kStatementLen = i::StrLength(statement_data[j]);
        int kTerminationLen = i::StrLength(termination_data[k]);
        int kSuffixLen = i::StrLength(context_data[i][1]);
        int kProgramSize = kPrefixLen + kStatementLen + kTerminationLen
            + kSuffixLen + i::StrLength("label: for (;;) {  }");

        // Plug the source code pieces together.
        i::Vector<char> program = i::Vector<char>::New(kProgramSize + 1);
        int length = i::OS::SNPrintF(program,
            "label: for (;;) { %s%s%s%s }",
            context_data[i][0],
            statement_data[j],
            termination_data[k],
            context_data[i][1]);
        CHECK(length == kProgramSize);
        i::Handle<i::String> source =
            FACTORY->NewStringFromAscii(i::CStrVector(program.start()));
        TestParserSync(source, true);
        TestParserSync(source, false);
      }
    }
  }
}

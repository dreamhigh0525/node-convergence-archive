// Copyright 2010 the V8 project authors. All rights reserved.
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
//
// Tests of profiles generator and utilities.

#include "v8.h"
#include "cpu-profiler-inl.h"
#include "cctest.h"
#include "../include/v8-profiler.h"

using i::CodeEntry;
using i::CpuProfile;
using i::CpuProfiler;
using i::CpuProfilesCollection;
using i::ProfileGenerator;
using i::ProfileNode;
using i::ProfilerEventsProcessor;
using i::TokenEnumerator;


TEST(StartStop) {
  CpuProfilesCollection profiles;
  ProfileGenerator generator(&profiles);
  ProfilerEventsProcessor processor(&generator);
  processor.Start();
  processor.Stop();
  processor.Join();
}

static v8::Persistent<v8::Context> env;

static void InitializeVM() {
  if (env.IsEmpty()) env = v8::Context::New();
  v8::HandleScope scope(env->GetIsolate());
  env->Enter();
}

static inline i::Address ToAddress(int n) {
  return reinterpret_cast<i::Address>(n);
}

static void EnqueueTickSampleEvent(ProfilerEventsProcessor* proc,
                                   i::Address frame1,
                                   i::Address frame2 = NULL,
                                   i::Address frame3 = NULL) {
  i::TickSample* sample = proc->TickSampleEvent();
  sample->pc = frame1;
  sample->tos = frame1;
  sample->frames_count = 0;
  if (frame2 != NULL) {
    sample->stack[0] = frame2;
    sample->frames_count = 1;
  }
  if (frame3 != NULL) {
    sample->stack[1] = frame3;
    sample->frames_count = 2;
  }
}

namespace {

class TestSetup {
 public:
  TestSetup()
      : old_flag_prof_browser_mode_(i::FLAG_prof_browser_mode) {
    i::FLAG_prof_browser_mode = false;
  }

  ~TestSetup() {
    i::FLAG_prof_browser_mode = old_flag_prof_browser_mode_;
  }

 private:
  bool old_flag_prof_browser_mode_;
};

}  // namespace

TEST(CodeEvents) {
  InitializeVM();
  i::Isolate* isolate = i::Isolate::Current();
  i::Heap* heap = isolate->heap();
  i::Factory* factory = isolate->factory();
  TestSetup test_setup;
  CpuProfilesCollection profiles;
  profiles.StartProfiling("", 1, false);
  ProfileGenerator generator(&profiles);
  ProfilerEventsProcessor processor(&generator);
  processor.Start();

  // Enqueue code creation events.
  i::HandleScope scope(isolate);
  const char* aaa_str = "aaa";
  i::Handle<i::String> aaa_name = factory->NewStringFromAscii(
      i::Vector<const char>(aaa_str, i::StrLength(aaa_str)));
  processor.CodeCreateEvent(i::Logger::FUNCTION_TAG,
                            *aaa_name,
                            heap->empty_string(),
                            0,
                            ToAddress(0x1000),
                            0x100,
                            ToAddress(0x10000));
  processor.CodeCreateEvent(i::Logger::BUILTIN_TAG,
                            "bbb",
                            ToAddress(0x1200),
                            0x80);
  processor.CodeCreateEvent(i::Logger::STUB_TAG, 5, ToAddress(0x1300), 0x10);
  processor.CodeCreateEvent(i::Logger::BUILTIN_TAG,
                            "ddd",
                            ToAddress(0x1400),
                            0x80);
  processor.CodeMoveEvent(ToAddress(0x1400), ToAddress(0x1500));
  processor.CodeCreateEvent(i::Logger::STUB_TAG, 3, ToAddress(0x1600), 0x10);
  processor.CodeCreateEvent(i::Logger::STUB_TAG, 4, ToAddress(0x1605), 0x10);
  // Enqueue a tick event to enable code events processing.
  EnqueueTickSampleEvent(&processor, ToAddress(0x1000));

  processor.Stop();
  processor.Join();

  // Check the state of profile generator.
  CodeEntry* entry1 = generator.code_map()->FindEntry(ToAddress(0x1000));
  CHECK_NE(NULL, entry1);
  CHECK_EQ(aaa_str, entry1->name());
  CodeEntry* entry2 = generator.code_map()->FindEntry(ToAddress(0x1200));
  CHECK_NE(NULL, entry2);
  CHECK_EQ("bbb", entry2->name());
  CodeEntry* entry3 = generator.code_map()->FindEntry(ToAddress(0x1300));
  CHECK_NE(NULL, entry3);
  CHECK_EQ("5", entry3->name());
  CHECK_EQ(NULL, generator.code_map()->FindEntry(ToAddress(0x1400)));
  CodeEntry* entry4 = generator.code_map()->FindEntry(ToAddress(0x1500));
  CHECK_NE(NULL, entry4);
  CHECK_EQ("ddd", entry4->name());
  CHECK_EQ(NULL, generator.code_map()->FindEntry(ToAddress(0x1600)));
}


template<typename T>
static int CompareProfileNodes(const T* p1, const T* p2) {
  return strcmp((*p1)->entry()->name(), (*p2)->entry()->name());
}

TEST(TickEvents) {
  TestSetup test_setup;
  CpuProfilesCollection profiles;
  profiles.StartProfiling("", 1, false);
  ProfileGenerator generator(&profiles);
  ProfilerEventsProcessor processor(&generator);
  processor.Start();

  processor.CodeCreateEvent(i::Logger::BUILTIN_TAG,
                            "bbb",
                            ToAddress(0x1200),
                            0x80);
  processor.CodeCreateEvent(i::Logger::STUB_TAG, 5, ToAddress(0x1300), 0x10);
  processor.CodeCreateEvent(i::Logger::BUILTIN_TAG,
                            "ddd",
                            ToAddress(0x1400),
                            0x80);
  EnqueueTickSampleEvent(&processor, ToAddress(0x1210));
  EnqueueTickSampleEvent(&processor, ToAddress(0x1305), ToAddress(0x1220));
  EnqueueTickSampleEvent(&processor,
                         ToAddress(0x1404),
                         ToAddress(0x1305),
                         ToAddress(0x1230));

  processor.Stop();
  processor.Join();
  CpuProfile* profile =
      profiles.StopProfiling(TokenEnumerator::kNoSecurityToken, "", 1);
  CHECK_NE(NULL, profile);

  // Check call trees.
  const i::List<ProfileNode*>* top_down_root_children =
      profile->top_down()->root()->children();
  CHECK_EQ(1, top_down_root_children->length());
  CHECK_EQ("bbb", top_down_root_children->last()->entry()->name());
  const i::List<ProfileNode*>* top_down_bbb_children =
      top_down_root_children->last()->children();
  CHECK_EQ(1, top_down_bbb_children->length());
  CHECK_EQ("5", top_down_bbb_children->last()->entry()->name());
  const i::List<ProfileNode*>* top_down_stub_children =
      top_down_bbb_children->last()->children();
  CHECK_EQ(1, top_down_stub_children->length());
  CHECK_EQ("ddd", top_down_stub_children->last()->entry()->name());
  const i::List<ProfileNode*>* top_down_ddd_children =
      top_down_stub_children->last()->children();
  CHECK_EQ(0, top_down_ddd_children->length());
}


// http://crbug/51594
// This test must not crash.
TEST(CrashIfStoppingLastNonExistentProfile) {
  InitializeVM();
  TestSetup test_setup;
  CpuProfiler* profiler = i::Isolate::Current()->cpu_profiler();
  profiler->StartProfiling("1");
  profiler->StopProfiling("2");
  profiler->StartProfiling("1");
  profiler->StopProfiling("");
}


// http://code.google.com/p/v8/issues/detail?id=1398
// Long stacks (exceeding max frames limit) must not be erased.
TEST(Issue1398) {
  TestSetup test_setup;
  CpuProfilesCollection profiles;
  profiles.StartProfiling("", 1, false);
  ProfileGenerator generator(&profiles);
  ProfilerEventsProcessor processor(&generator);
  processor.Start();

  processor.CodeCreateEvent(i::Logger::BUILTIN_TAG,
                            "bbb",
                            ToAddress(0x1200),
                            0x80);

  i::TickSample* sample = processor.TickSampleEvent();
  sample->pc = ToAddress(0x1200);
  sample->tos = 0;
  sample->frames_count = i::TickSample::kMaxFramesCount;
  for (int i = 0; i < sample->frames_count; ++i) {
    sample->stack[i] = ToAddress(0x1200);
  }

  processor.Stop();
  processor.Join();
  CpuProfile* profile =
      profiles.StopProfiling(TokenEnumerator::kNoSecurityToken, "", 1);
  CHECK_NE(NULL, profile);

  int actual_depth = 0;
  const ProfileNode* node = profile->top_down()->root();
  while (node->children()->length() > 0) {
    node = node->children()->last();
    ++actual_depth;
  }

  CHECK_EQ(1 + i::TickSample::kMaxFramesCount, actual_depth);  // +1 for PC.
}


TEST(DeleteAllCpuProfiles) {
  InitializeVM();
  TestSetup test_setup;
  CpuProfiler* profiler = i::Isolate::Current()->cpu_profiler();
  CHECK_EQ(0, profiler->GetProfilesCount());
  profiler->DeleteAllProfiles();
  CHECK_EQ(0, profiler->GetProfilesCount());

  profiler->StartProfiling("1");
  profiler->StopProfiling("1");
  CHECK_EQ(1, profiler->GetProfilesCount());
  profiler->DeleteAllProfiles();
  CHECK_EQ(0, profiler->GetProfilesCount());
  profiler->StartProfiling("1");
  profiler->StartProfiling("2");
  profiler->StopProfiling("2");
  profiler->StopProfiling("1");
  CHECK_EQ(2, profiler->GetProfilesCount());
  profiler->DeleteAllProfiles();
  CHECK_EQ(0, profiler->GetProfilesCount());

  // Test profiling cancellation by the 'delete' command.
  profiler->StartProfiling("1");
  profiler->StartProfiling("2");
  CHECK_EQ(0, profiler->GetProfilesCount());
  profiler->DeleteAllProfiles();
  CHECK_EQ(0, profiler->GetProfilesCount());
}


TEST(DeleteCpuProfile) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CHECK_EQ(0, v8::CpuProfiler::GetProfilesCount());
  v8::Local<v8::String> name1 = v8::String::New("1");
  v8::CpuProfiler::StartProfiling(name1);
  const v8::CpuProfile* p1 = v8::CpuProfiler::StopProfiling(name1);
  CHECK_NE(NULL, p1);
  CHECK_EQ(1, v8::CpuProfiler::GetProfilesCount());
  unsigned uid1 = p1->GetUid();
  CHECK_EQ(p1, v8::CpuProfiler::FindProfile(uid1));
  const_cast<v8::CpuProfile*>(p1)->Delete();
  CHECK_EQ(0, v8::CpuProfiler::GetProfilesCount());
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid1));

  v8::Local<v8::String> name2 = v8::String::New("2");
  v8::CpuProfiler::StartProfiling(name2);
  const v8::CpuProfile* p2 = v8::CpuProfiler::StopProfiling(name2);
  CHECK_NE(NULL, p2);
  CHECK_EQ(1, v8::CpuProfiler::GetProfilesCount());
  unsigned uid2 = p2->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid2));
  CHECK_EQ(p2, v8::CpuProfiler::FindProfile(uid2));
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid1));
  v8::Local<v8::String> name3 = v8::String::New("3");
  v8::CpuProfiler::StartProfiling(name3);
  const v8::CpuProfile* p3 = v8::CpuProfiler::StopProfiling(name3);
  CHECK_NE(NULL, p3);
  CHECK_EQ(2, v8::CpuProfiler::GetProfilesCount());
  unsigned uid3 = p3->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid3));
  CHECK_EQ(p3, v8::CpuProfiler::FindProfile(uid3));
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid1));
  const_cast<v8::CpuProfile*>(p2)->Delete();
  CHECK_EQ(1, v8::CpuProfiler::GetProfilesCount());
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid2));
  CHECK_EQ(p3, v8::CpuProfiler::FindProfile(uid3));
  const_cast<v8::CpuProfile*>(p3)->Delete();
  CHECK_EQ(0, v8::CpuProfiler::GetProfilesCount());
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid3));
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid2));
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid1));
}


TEST(DeleteCpuProfileDifferentTokens) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CHECK_EQ(0, v8::CpuProfiler::GetProfilesCount());
  v8::Local<v8::String> name1 = v8::String::New("1");
  v8::CpuProfiler::StartProfiling(name1);
  const v8::CpuProfile* p1 = v8::CpuProfiler::StopProfiling(name1);
  CHECK_NE(NULL, p1);
  CHECK_EQ(1, v8::CpuProfiler::GetProfilesCount());
  unsigned uid1 = p1->GetUid();
  CHECK_EQ(p1, v8::CpuProfiler::FindProfile(uid1));
  v8::Local<v8::String> token1 = v8::String::New("token1");
  const v8::CpuProfile* p1_t1 = v8::CpuProfiler::FindProfile(uid1, token1);
  CHECK_NE(NULL, p1_t1);
  CHECK_NE(p1, p1_t1);
  CHECK_EQ(1, v8::CpuProfiler::GetProfilesCount());
  const_cast<v8::CpuProfile*>(p1)->Delete();
  CHECK_EQ(0, v8::CpuProfiler::GetProfilesCount());
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid1));
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid1, token1));
  const_cast<v8::CpuProfile*>(p1_t1)->Delete();
  CHECK_EQ(0, v8::CpuProfiler::GetProfilesCount());

  v8::Local<v8::String> name2 = v8::String::New("2");
  v8::CpuProfiler::StartProfiling(name2);
  v8::Local<v8::String> token2 = v8::String::New("token2");
  const v8::CpuProfile* p2_t2 = v8::CpuProfiler::StopProfiling(name2, token2);
  CHECK_NE(NULL, p2_t2);
  CHECK_EQ(1, v8::CpuProfiler::GetProfilesCount());
  unsigned uid2 = p2_t2->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid2));
  const v8::CpuProfile* p2 = v8::CpuProfiler::FindProfile(uid2);
  CHECK_NE(p2_t2, p2);
  v8::Local<v8::String> name3 = v8::String::New("3");
  v8::CpuProfiler::StartProfiling(name3);
  const v8::CpuProfile* p3 = v8::CpuProfiler::StopProfiling(name3);
  CHECK_NE(NULL, p3);
  CHECK_EQ(2, v8::CpuProfiler::GetProfilesCount());
  unsigned uid3 = p3->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid3));
  CHECK_EQ(p3, v8::CpuProfiler::FindProfile(uid3));
  const_cast<v8::CpuProfile*>(p2_t2)->Delete();
  CHECK_EQ(1, v8::CpuProfiler::GetProfilesCount());
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid2));
  CHECK_EQ(p3, v8::CpuProfiler::FindProfile(uid3));
  const_cast<v8::CpuProfile*>(p2)->Delete();
  CHECK_EQ(1, v8::CpuProfiler::GetProfilesCount());
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid2));
  CHECK_EQ(p3, v8::CpuProfiler::FindProfile(uid3));
  const_cast<v8::CpuProfile*>(p3)->Delete();
  CHECK_EQ(0, v8::CpuProfiler::GetProfilesCount());
  CHECK_EQ(NULL, v8::CpuProfiler::FindProfile(uid3));
}

#include "node.h"
#include "child_process.h"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

using namespace v8;
using namespace node;

#define PID_SYMBOL String::NewSymbol("pid")

Persistent<FunctionTemplate> ChildProcess::constructor_template;

void
ChildProcess::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(ChildProcess::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "spawn", ChildProcess::Spawn);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "write", ChildProcess::Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", ChildProcess::Close);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "kill", ChildProcess::Kill);

  target->Set(String::NewSymbol("ChildProcess"), constructor_template->GetFunction());
}

Handle<Value>
ChildProcess::New (const Arguments& args)
{
  HandleScope scope;

  ChildProcess *p = new ChildProcess();
  p->Wrap(args.Holder());

  return args.This();
}

Handle<Value>
ChildProcess::Spawn (const Arguments& args)
{
  if (args.Length() == 0 || !args[0]->IsString()) {
    return ThrowException(String::New("Bad argument."));
  }

  HandleScope scope;
  ChildProcess *child = ObjectWrap::Unwrap<ChildProcess>(args.Holder());

  String::Utf8Value command(args[0]->ToString());

  int r = child->Spawn(*command);
  if (r != 0) {
    return ThrowException(String::New("Error spawning"));
  }

  child->handle_->Set(PID_SYMBOL, Integer::New(child->pid_));

  return Undefined();
}

Handle<Value>
ChildProcess::Write (const Arguments& args)
{
  HandleScope scope;
  ChildProcess *child = ObjectWrap::Unwrap<ChildProcess>(args.Holder());
  assert(child);

  enum encoding enc = ParseEncoding(args[1]);
  ssize_t len = DecodeBytes(args[0], enc);

  if (len < 0) {
    Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
    return ThrowException(exception);
  }

  char buf[len];
  ssize_t written = DecodeWrite(buf, len, args[0], enc);
  
  assert(written == len);

  return child->Write(buf, len) == 0 ? True() : False();
}

Handle<Value>
ChildProcess::Kill (const Arguments& args)
{
  HandleScope scope;
  ChildProcess *child = ObjectWrap::Unwrap<ChildProcess>(args.Holder());
  assert(child);

  int sig = SIGTERM;
  if (args[0]->IsInt32()) sig = args[0]->Int32Value();

  if (child->Kill(sig) != 0) {
    return ThrowException(String::New("ChildProcess already dead"));
  }

  return Undefined();
}

Handle<Value>
ChildProcess::Close (const Arguments& args)
{
  HandleScope scope;
  ChildProcess *child = ObjectWrap::Unwrap<ChildProcess>(args.Holder());
  assert(child);
  return child->Close() == 0 ? True() : False();
}

void
ChildProcess::reader_closed (evcom_reader *r)
{
  ChildProcess *child = static_cast<ChildProcess*> (r->data);
  if (r == &child->stdout_reader_) {
    child->stdout_fd_ = -1;
  } else {
    assert(r == &child->stderr_reader_);
    child->stderr_fd_ = -1;
  }
  evcom_reader_detach(r);
  child->MaybeShutdown();
}

void
ChildProcess::stdin_closed (evcom_writer *w)
{
  ChildProcess *child = static_cast<ChildProcess*> (w->data);
  assert(w == &child->stdin_writer_);
  child->stdin_fd_ = -1;
  evcom_writer_detach(w);
  child->MaybeShutdown();
}

void
ChildProcess::on_read (evcom_reader *r, const void *buf, size_t len)
{
  ChildProcess *child = static_cast<ChildProcess*> (r->data);
  HandleScope scope;

  bool isSTDOUT = (r == &child->stdout_reader_);
  enum encoding encoding = isSTDOUT ? child->stdout_encoding_ : child->stderr_encoding_;

  Local<Value> data = Encode(buf, len, encoding);
  child->Emit(isSTDOUT ? "output" : "error", 1, &data);
  child->MaybeShutdown();
}

ChildProcess::ChildProcess ()
  : EventEmitter()
{
  evcom_reader_init(&stdout_reader_);
  stdout_reader_.data     = this;
  stdout_reader_.on_read  = on_read;
  stdout_reader_.on_close = reader_closed;

  evcom_reader_init(&stderr_reader_);
  stderr_reader_.data     = this;
  stderr_reader_.on_read  = on_read;
  stderr_reader_.on_close = reader_closed;

  evcom_writer_init(&stdin_writer_);
  stdin_writer_.data      = this;
  stdin_writer_.on_close  = stdin_closed;

  ev_init(&child_watcher_, ChildProcess::OnCHLD);
  child_watcher_.data = this;

  stdout_fd_ = -1;
  stderr_fd_ = -1;
  stdin_fd_ = -1;

  stdout_encoding_ = UTF8;
  stderr_encoding_ = UTF8;

  got_chld_ = false;
  exit_code_ = 0;

  pid_ = 0;
}

ChildProcess::~ChildProcess ()
{
  Shutdown();
}

void
ChildProcess::Shutdown ()
{
  if (stdin_fd_ >= 0) {
    evcom_writer_close(&stdin_writer_);
  }

  if (stdin_fd_  >= 0) close(stdin_fd_);
  if (stdout_fd_ >= 0) close(stdout_fd_);
  if (stderr_fd_ >= 0) close(stderr_fd_);

  stdin_fd_ = -1;
  stdout_fd_ = -1;
  stderr_fd_ = -1;

  evcom_writer_detach(&stdin_writer_);
  evcom_reader_detach(&stdout_reader_);
  evcom_reader_detach(&stderr_reader_);

  ev_child_stop(EV_DEFAULT_UC_ &child_watcher_);

  /* XXX Kill the PID? */
  pid_ = 0;
}

static inline int
SetNonBlocking (int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (r != 0) {
    perror("SetNonBlocking()");
  }
  return r;
}

int
ChildProcess::Spawn (const char *command)
{
  assert(pid_ == 0);
  assert(stdout_fd_ == -1);
  assert(stderr_fd_ == -1);
  assert(stdin_fd_ == -1);

  int stdout_pipe[2], stdin_pipe[2], stderr_pipe[2];

  /* An implementation of popen(), basically */
  if (pipe(stdout_pipe) < 0) {
    perror("pipe()");
    return -1;
  }

  if (pipe(stderr_pipe) < 0) {
    perror("pipe()");
    return -2;
  }

  if (pipe(stdin_pipe) < 0) {
    perror("pipe()");
    return -3;
  }

  switch (pid_ = vfork()) {
    case -1: // Error.
      Shutdown();
      return -4;

    case 0: // Child.
      close(stdout_pipe[0]); // close read end
      dup2(stdout_pipe[1], STDOUT_FILENO);

      close(stderr_pipe[0]); // close read end
      dup2(stderr_pipe[1], STDERR_FILENO);

      close(stdin_pipe[1]); // close write end
      dup2(stdin_pipe[0],  STDIN_FILENO);

      execl("/bin/sh", "sh", "-c", command, (char *)NULL);
      _exit(127);
  }

  // Parent.

  ev_child_set(&child_watcher_, pid_, 0);
  ev_child_start(EV_DEFAULT_UC_ &child_watcher_);

  close(stdout_pipe[1]);
  stdout_fd_ = stdout_pipe[0];
  SetNonBlocking(stdout_fd_);

  close(stderr_pipe[1]);
  stderr_fd_ = stderr_pipe[0];
  SetNonBlocking(stderr_fd_);

  close(stdin_pipe[0]);
  stdin_fd_ = stdin_pipe[1];
  SetNonBlocking(stdin_fd_);

  evcom_reader_set(&stdout_reader_, stdout_fd_);
  evcom_reader_attach(EV_DEFAULT_UC_ &stdout_reader_);

  evcom_reader_set(&stderr_reader_, stderr_fd_);
  evcom_reader_attach(EV_DEFAULT_UC_ &stderr_reader_);

  evcom_writer_set(&stdin_writer_, stdin_fd_);
  evcom_writer_attach(EV_DEFAULT_UC_ &stdin_writer_);

  Attach();

  return 0;
}

void
ChildProcess::OnCHLD (EV_P_ ev_child *watcher, int revents)
{
  ev_child_stop(EV_A_ watcher);
  ChildProcess *child = static_cast<ChildProcess*>(watcher->data);

  assert(revents == EV_CHILD);
  assert(child->pid_ == watcher->rpid);
  assert(&child->child_watcher_ == watcher);

  child->got_chld_ = true;
  child->exit_code_ = watcher->rstatus;

  if (child->stdin_fd_  >= 0) evcom_writer_close(&child->stdin_writer_);

  child->MaybeShutdown();
}

int
ChildProcess::Write (const char *str, size_t len)
{
  if (stdin_fd_ < 0 || got_chld_) return -1;
  evcom_writer_write(&stdin_writer_, str, len);
  return 0;
}

int
ChildProcess::Close (void)
{
  if (stdin_fd_ < 0 || got_chld_) return -1;
  evcom_writer_close(EV_DEFAULT_UC_ &stdin_writer_);
  return 0;
}

int
ChildProcess::Kill (int sig)
{
  if (got_chld_ || pid_ == 0) return -1;
  return kill(pid_, sig);
}

void
ChildProcess::MaybeShutdown (void)
{
  if (stdout_fd_ < 0 && stderr_fd_ < 0 && got_chld_) {
    HandleScope scope;
    Handle<Value> argv[1] = { Integer::New(exit_code_) };
    Emit("exit", 1, argv);
    Shutdown();
    Detach();
  }
}

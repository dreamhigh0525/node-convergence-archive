#include "net.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace v8;
using namespace node;

#define ON_CONNECT_SYMBOL String::NewSymbol("onConnect")
#define ON_CONNECTION_SYMBOL String::NewSymbol("onConnection")
#define ON_READ_SYMBOL String::NewSymbol("onRead")

static const struct addrinfo tcp_hints = 
/* ai_flags      */ { AI_PASSIVE
/* ai_family     */ , AF_UNSPEC
/* ai_socktype   */ , SOCK_STREAM
/* ai_protocol   */ , 0
/* ai_addrlen    */ , 0
/* ai_addr       */ , NULL
/* ai_canonname  */ , NULL
/* ai_next       */ , NULL
                    };


Server::Server (Handle<Object> handle, int backlog)
  : ObjectWrap(handle)
{
  //HandleScope scope;
  oi_server_init(&server_, backlog);
  server_.on_connection = Server::OnConnection;
//  server_.on_error      = Server::OnError;
  server_.data = this;
}

Server::~Server ()
{
  //HandleScope scope;
  oi_server_close(&server_);
  oi_server_detach(&server_);
}

Handle<Value>
Server::New (const Arguments& args)
{
  HandleScope scope;

  int backlog = 1024; // default
  if (args.Length() > 0 && args[0]->IsNumber()) 
    backlog = args[0]->IntegerValue();

  Server *server = new Server(args.Holder(), backlog);
  if(server == NULL)
    return Undefined(); // XXX raise error?

  return args.This();
}

Handle<Value>
Server::ListenTCP (const Arguments& args)
{
  if (args.Length() < 2) return Undefined();
  HandleScope scope;

  Server *server = NODE_UNWRAP(Server, args.Holder());

  String::AsciiValue port(args[0]);

  int callback_index = 1;
  char *host = NULL; 
  if (args[1]->IsString()) {
    callback_index = 2;
    String::AsciiValue host_v(args[1]->ToString());
    if(args[1]->IsString()) host = *host_v;
  }

  // For servers call getaddrinfo inline. This is blocking but it shouldn't
  // matter--ever. If someone actually complains then simply swap it out
  // with a libeio call.
  struct addrinfo *address = NULL;
  int r = getaddrinfo(host, *port, &tcp_hints, &address);
  if (r != 0)
    return ThrowException(String::New("Error looking up hostname"));

  if (!args[callback_index]->IsFunction())
    return ThrowException(String::New("Must supply onConnection callback"));

  server->handle_->Set(ON_CONNECTION_SYMBOL, args[callback_index]);

  r = oi_server_listen(&server->server_, address);
  if (r != 0)
    return ThrowException(String::New("Error listening on port"));
  oi_server_attach(EV_DEFAULT_UC_ &server->server_);

  freeaddrinfo(address);

  return Undefined();
}

Handle<Value>
Server::Close (const Arguments& args)
{
  HandleScope scope;
  Server *server = NODE_UNWRAP(Server, args.Holder());
  oi_server_close(&server->server_);
  return Undefined();
}

oi_socket*
Server::OnConnection (oi_server *s, struct sockaddr *remote_addr, socklen_t remote_addr_len)
{
  Server *server = static_cast<Server*> (s->data);
  HandleScope scope;

  Socket *socket = Socket::NewConnection(60.0);

  Local<Value> callback_v = server->handle_->Get(ON_CONNECTION_SYMBOL);
  if (!callback_v->IsFunction())
    return NULL; // produce error?

  Local<Function> callback = Local<Function>::Cast(callback_v);
  const int argc = 1;
  Local<Value> argv[argc];
  argv[0] =  Local<Value>::New(socket->handle_);
  callback->Call(server->handle_, argc, argv);

  return &socket->socket_;
}

Handle<Value>
Socket::New(const Arguments& args)
{
  if (args.Length() > 1)
    return Undefined();

  HandleScope scope;

  // Default options 
  double timeout = 60.0; // in seconds
  enum {UTF8, RAW} encoding ;

  // Set options from argument.
  if (args.Length() == 1 && args[0]->IsObject()) {
    Local<Object> options = args[0]->ToObject();
    Local<Value> timeout_value = options->Get(String::NewSymbol("timeout"));
    Local<Value> encoding_value = options->Get(String::NewSymbol("encoding"));

    if (timeout_value->IsNumber()) {
      // timeout is specified in milliseconds like other time
      // values in javascript
      timeout = timeout_value->NumberValue() / 1000;
    }

    if (encoding_value->IsString()) {
      Local<String> encoding_string = encoding_value->ToString();
      char buf[5]; // need enough room for "utf8" or "raw"
      encoding_string->WriteAscii(buf, 0, 4);
      buf[4] = '\0';
      if(strcasecmp(buf, "utf8") == 0) encoding = UTF8;
    }
  }

  Socket *s = new Socket(args.Holder(), timeout);
  if(s == NULL)
    return Undefined(); // XXX raise error?

  return args.This();
}

void
Socket::SetEncoding (Handle<Value> encoding_value)
{
  if (encoding_value->IsString()) {
    HandleScope scope;
    Local<String> encoding_string = encoding_value->ToString();
    char buf[5]; // need enough room for "utf8" or "raw"
    encoding_string->WriteAscii(buf, 0, 4);
    buf[4] = '\0';
    if(strcasecmp(buf, "utf8") == 0)
      encoding_ = UTF8;
    else
      encoding_ = RAW;
  }
}

Handle<Value>
Socket::ConnectTCP (const Arguments& args)
{
  if (args.Length() < 1)
    return Undefined();

  HandleScope scope;
  Socket *socket = NODE_UNWRAP(Socket, args.Holder());

  String::AsciiValue port(args[0]);
  socket->port_ = strdup(*port);

  assert(socket->host_ == NULL);
  String::AsciiValue host_v(args[1]->ToString());
  if(args[1]->IsString()) {
    socket->host_ = strdup(*host_v);
  }

  if(args[2]->IsFunction()) {
    socket->handle_->Set(ON_CONNECT_SYMBOL , args[2]);
  }

  /* For the moment I will do DNS lookups in the thread pool. This is
   * sub-optimal and cannot handle massive numbers of requests but it is 
   * quite portable. 
   * In the future I will move to a system using adns or udns: 
   * http://lists.schmorp.de/pipermail/libev/2009q1/000632.html
   */
  eio_warmup();
  eio_req *req = eio_custom (Socket::Resolve, EIO_PRI_DEFAULT, Socket::AfterResolve, socket);

  return Undefined();
}

/* This function is executed in the thread pool. It cannot touch anything! */
int
Socket::Resolve (eio_req *req) 
{
  Socket *socket = static_cast<Socket*> (req->data);
  struct addrinfo *address = NULL;

  req->result = getaddrinfo(socket->host_, socket->port_, &tcp_hints, &address);

  req->ptr2 = address;

  free(socket->host_);
  socket->host_ = NULL;

  free(socket->port_);
  socket->port_ = NULL;

  return 0;
}

int
Socket::AfterResolve (eio_req *req) 
{
  Socket *socket = static_cast<Socket*> (req->data);
  struct addrinfo *address = static_cast<struct addrinfo *>(req->ptr2);
  req->ptr2 = NULL;

  int r = 0;
  if (req->result == 0) {
    r = oi_socket_connect (&socket->socket_, address);
  }
  if (address)
    freeaddrinfo(address); 

  // no error. return.
  if(r == 0 && req->result == 0) {
    oi_socket_attach (EV_DEFAULT_UC_ &socket->socket_);
    return 0;
  }

  HandleScope scope;
  Handle<Value> onconnect_value = socket->handle_->Get(ON_CONNECT_SYMBOL);
  if (!onconnect_value->IsFunction()) return 0; 
  Handle<Function> onconnect = Handle<Function>::Cast(onconnect_value);

  TryCatch try_catch;
  const int argc = 1;
  Local<Value> argv[argc];
  argv[0] = Integer::New(r | req->result); // FIXME very stupid error code.

  onconnect->Call(socket->handle_, argc, argv);
  if(try_catch.HasCaught())
    fatal_exception(try_catch);

  return 0;
}

Handle<Value>
Socket::Close (const Arguments& args) 
{
  HandleScope scope;
  Socket *socket = NODE_UNWRAP(Socket, args.Holder());
  oi_socket_close(&socket->socket_);
  return Undefined();
}

Socket::Socket(Handle<Object> handle, double timeout)
  : ObjectWrap(handle)
{
  //HandleScope scope;
  oi_socket_init(&socket_, timeout);
  socket_.on_connect = Socket::OnConnect;
  socket_.on_read    = Socket::OnRead;
//  socket_.on_drain   = Socket::OnDrain;
//  socket_.on_error   = Socket::OnError;
  socket_.on_close   = Socket::OnClose;
  socket_.on_timeout = Socket::OnTimeout;
  socket_.data = this;

  encoding_ = UTF8; // default encoding.
  host_ = NULL;
  port_ = NULL;
}

Socket*
Socket::NewConnection (double timeout)
{
  HandleScope scope;

  Local<Object> socket_handle = socket_template->GetFunction()->NewInstance();
  Socket *socket = new Socket(socket_handle, 60.0);
  socket->handle_->Delete(String::NewSymbol("connectTCP"));

  return socket;
}

Socket::~Socket ()
{
  oi_socket_close(&socket_);
  oi_socket_detach(&socket_);
  free(host_);
  free(port_);

  //HandleScope scope;
  handle_->Delete(String::NewSymbol("write"));
  handle_->Delete(String::NewSymbol("close"));
}

Handle<Value>
Socket::SetEncoding (const Arguments& args) 
{
  HandleScope scope;
  Socket *socket = NODE_UNWRAP(Socket, args.Holder());
  socket->SetEncoding(args[0]);
  return Undefined();
}

Handle<Value>
Socket::Write (const Arguments& args) 
{
  HandleScope scope;

  Socket *socket = NODE_UNWRAP(Socket, args.Holder());

  // TODO support a callback using buf->on_release
 
  if (args[0] == Null()) {
    oi_socket_write_eof(&socket->socket_);

  } else if (args[0]->IsString()) {
    // utf8 encoding
    Local<String> s = args[0]->ToString();
    size_t length = s->Utf8Length();
    oi_buf *buf = oi_buf_new2(length);
    s->WriteUtf8(buf->base, length);
    oi_socket_write(&socket->socket_, buf);

  } else if (args[0]->IsArray()) {
    // raw encoding
    Handle<Array> array = Handle<Array>::Cast(args[0]);
    size_t length = array->Length();
    oi_buf *buf = oi_buf_new2(length);
    for (int i = 0; i < length; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf->base[i] = int_value->IntegerValue();
    }
    oi_socket_write(&socket->socket_, buf);

  } else return ThrowException(String::New("Bad argument"));

  return Undefined();
}

void
Socket::OnConnect (oi_socket *s)
{
  Socket *socket = static_cast<Socket*> (s->data);

  HandleScope scope;

  Handle<Value> on_connect_value = socket->handle_->Get(ON_CONNECT_SYMBOL);
  if (!on_connect_value->IsFunction())
    return; 
  Handle<Function> on_connect = Handle<Function>::Cast(on_connect_value);

  TryCatch try_catch;
  const int argc = 1;
  Local<Value> argv[argc];
  argv[0] = Integer::New(0); 

  Handle<Value> r = on_connect->Call(socket->handle_, argc, argv);

  if(try_catch.HasCaught())
    fatal_exception(try_catch);
}

void
Socket::OnRead (oi_socket *s, const void *buf, size_t count)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> onread_value = socket->handle_->Get(ON_READ_SYMBOL);
  if (!onread_value->IsFunction()) return; 
  Handle<Function> onread = Handle<Function>::Cast(onread_value);

  const int argc = 1;
  Handle<Value> argv[argc];

  if(count) {
    if(socket->encoding_ == UTF8) {
      // utf8 encoding
      Handle<String> chunk = String::New((const char*)buf, count);
      argv[0] = chunk;
    } else {
      // raw encoding
      Local<Array> array = Array::New(count);
      for (int i = 0; i < count; i++) {
        char val = static_cast<const char*>(buf)[i];
        array->Set(Integer::New(i), Integer::New(val));
      }
      argv[0] = array;
    }
  } else {
    argv[0] = Local<Value>::New(Null());
  }

  TryCatch try_catch;

  Handle<Value> r = onread->Call(socket->handle_, argc, argv);

  if(try_catch.HasCaught())
    fatal_exception(try_catch);
}

void
Socket::OnClose (oi_socket *s)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> onclose_value = socket->handle_->Get( String::NewSymbol("onClose") );
  if (!onclose_value->IsFunction()) {
    delete socket;
    return;
  }
  Handle<Function> onclose = Handle<Function>::Cast(onclose_value);

  TryCatch try_catch;

  Handle<Value> r = onclose->Call(socket->handle_, 0, NULL);

  if(try_catch.HasCaught())
    fatal_exception(try_catch);

  delete socket;
}

void
Socket::OnDrain (oi_socket *s)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> ondrain_value = socket->handle_->Get( String::NewSymbol("onDrain") );
  if (!ondrain_value->IsFunction()) return; 
  Handle<Function> ondrain = Handle<Function>::Cast(ondrain_value);

  TryCatch try_catch;

  Handle<Value> r = ondrain->Call(socket->handle_, 0, NULL);

  if(try_catch.HasCaught())
    fatal_exception(try_catch);
}


void
Socket::OnError (oi_socket *s, oi_error e)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> onerror_value = socket->handle_->Get( String::NewSymbol("onError") );
  if (!onerror_value->IsFunction()) return; 
  Handle<Function> onerror = Handle<Function>::Cast(onerror_value);

  TryCatch try_catch;

  Handle<Value> r = onerror->Call(socket->handle_, 0, NULL);

  if(try_catch.HasCaught())
    fatal_exception(try_catch);
}

void
Socket::OnTimeout (oi_socket *s)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> ontimeout_value = socket->handle_->Get( String::NewSymbol("onTimeout") );
  if (!ontimeout_value->IsFunction()) return; 
  Handle<Function> ontimeout = Handle<Function>::Cast(ontimeout_value);

  TryCatch try_catch;

  Handle<Value> r = ontimeout->Call(socket->handle_, 0, NULL);

  if(try_catch.HasCaught())
    fatal_exception(try_catch);
}

void
Socket::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> template_local = FunctionTemplate::New(Socket::New);
  socket_template = Persistent<FunctionTemplate>::New(template_local);
  socket_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("Socket"), socket_template->GetFunction());

  NODE_SET_METHOD(socket_template->InstanceTemplate(), "connectTCP", Socket::ConnectTCP);
  //NODE_SET_METHOD(socket_template->InstanceTemplate(), "connectUNIX", Socket::ConnectUNIX);
  NODE_SET_METHOD(socket_template->InstanceTemplate(), "write", Socket::Write);
  NODE_SET_METHOD(socket_template->InstanceTemplate(), "close", Socket::Close);
  NODE_SET_METHOD(socket_template->InstanceTemplate(), "setEncoding", Socket::SetEncoding);
}

void
Server::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> server_template = FunctionTemplate::New(Server::New);
  server_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("Server"), server_template->GetFunction());

  NODE_SET_METHOD(server_template->InstanceTemplate(), "listenTCP", Server::ListenTCP);
  NODE_SET_METHOD(server_template->InstanceTemplate(), "close", Server::Close);
}


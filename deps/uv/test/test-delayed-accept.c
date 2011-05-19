/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "../uv.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>


static char BUFFER[1024];

static int accept_cb_called = 0;
static int do_accept_called = 0;
static int close_cb_called = 0;
static int connect_cb_called = 0;


static void close_cb(uv_handle_t* handle, int status) {
  ASSERT(handle != NULL);
  ASSERT(status == 0);

  free(handle);

  close_cb_called++;
}


static void do_accept(uv_handle_t* timer_handle, int status) {
  uv_handle_t* server;
  uv_handle_t* accepted_handle = (uv_handle_t*)malloc(sizeof *accepted_handle);
  int r;

  ASSERT(timer_handle != NULL);
  ASSERT(status == 0);
  ASSERT(accepted_handle != NULL);

  server = (uv_handle_t*)timer_handle->data;
  r = uv_accept(server, accepted_handle, close_cb, NULL);
  ASSERT(r == 0);

  do_accept_called++;

  /* Immediately close the accepted handle. */
  r = uv_close(accepted_handle);
  ASSERT(r == 0);

  /* After accepting the two clients close the server handle */
  if (do_accept_called == 2) {
    r = uv_close(server);
    ASSERT(r == 0);
  }

  /* Dispose the timer. */
  r = uv_close(timer_handle);
  ASSERT(r == 0);
}


static void accept_cb(uv_handle_t* handle) {
  int r;
  uv_handle_t* timer_handle;

  timer_handle = (uv_handle_t*)malloc(sizeof *timer_handle);
  ASSERT(timer_handle != NULL);

  /* Accept the client after 1 second */
  r = uv_timer_init(timer_handle, close_cb, (void*)handle);
  ASSERT(r == 0);
  r = uv_timer_start(timer_handle, do_accept, 1000, 0);
  ASSERT(r == 0);

  accept_cb_called++;
}


static void start_server() {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", TEST_PORT);
  uv_handle_t* server = (uv_handle_t*)malloc(sizeof *server);
  int r;

  ASSERT(server != NULL);

  r = uv_tcp_init(server, close_cb, NULL);
  ASSERT(r == 0);

  r = uv_bind(server, (struct sockaddr*) &addr);
  ASSERT(r == 0);

  r = uv_listen(server, 128, accept_cb);
  ASSERT(r == 0);
}


static void read_cb(uv_handle_t* handle, int nread, uv_buf_t buf) {
  /* The server will not send anything, it should close gracefully. */
  ASSERT(handle != NULL);
  ASSERT(nread == -1);
  ASSERT(uv_last_error().code == UV_EOF);

  if (buf.base) {
    free(buf.base);
  }

  uv_close(handle);
}


static void connect_cb(uv_req_t* req, int status) {
  int r;

  ASSERT(req != NULL);
  ASSERT(status == 0);

  /* Not that the server will send anything, but otherwise we'll never know */
  /* when te server closes the connection. */
  r = uv_read_start(req->handle, read_cb);
  ASSERT(r == 0);

  connect_cb_called++;

  free(req);
}


static void client_connect() {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_handle_t* client = (uv_handle_t*)malloc(sizeof *client);
  uv_req_t* connect_req = (uv_req_t*)malloc(sizeof *connect_req);
  int r;

  ASSERT(client != NULL);
  ASSERT(connect_req != NULL);

  r = uv_tcp_init(client, close_cb, NULL);
  ASSERT(r == 0);

  uv_req_init(connect_req, client, connect_cb);
  r = uv_connect(connect_req, (struct sockaddr*)&addr);
  ASSERT(r == 0);
}


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t size) {
  uv_buf_t buf;
  buf.base = (char*)malloc(size);
  buf.len = size;
  return buf;
}



TEST_IMPL(delayed_accept) {
  uv_init(alloc_cb);

  start_server();

  client_connect();
  client_connect();

  uv_run();

  ASSERT(accept_cb_called == 2);
  ASSERT(do_accept_called == 2);
  ASSERT(connect_cb_called == 2);
  ASSERT(close_cb_called == 7);

  return 0;
}

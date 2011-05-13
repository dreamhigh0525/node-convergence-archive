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


typedef struct {
  uv_req_t req;
  uv_buf buf;
} write_req_t;


static uv_handle_t server;


static void after_write(uv_req_t* req, int status);
static void after_read(uv_handle_t* handle, int nread, uv_buf buf);
static void on_close(uv_handle_t* peer, int status);
static void on_accept(uv_handle_t* handle);


static void after_write(uv_req_t* req, int status) {
  write_req_t* wr;

  if (status) {
    uv_err_t err = uv_last_error();
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(err));
    ASSERT(0);
  }

  wr = (write_req_t*) req;

  /* Free the read/write buffer and the request */
  free(wr->buf.base);
  free(wr);
}


static void after_shutdown(uv_req_t* req, int status) {
  free(req);
}


static void after_read(uv_handle_t* handle, int nread, uv_buf buf) {
  write_req_t *wr;
  uv_req_t* req;

  if (nread < 0) {
    /* Error or EOF */
    ASSERT (uv_last_error().code == UV_EOF);

    if (buf.base) {
      free(buf.base);
    }

    req = (uv_req_t*) malloc(sizeof *req);
    uv_req_init(req, handle, after_shutdown);
    uv_shutdown(req);

    return;
  }

  if (nread == 0) {
    /* Everything OK, but nothing read. */
    free(buf.base);
    return;
  }

  wr = (write_req_t*) malloc(sizeof *wr);

  uv_req_init(&wr->req, handle, after_write);
  wr->buf.base = buf.base;
  wr->buf.len = nread;
  if (uv_write(&wr->req, &wr->buf, 1)) {
    FATAL("uv_write failed");
  }
}


static void on_close(uv_handle_t* peer, int status) {
  if (status != 0) {
    fprintf(stdout, "Socket error\n");
  }
}


static void on_accept(uv_handle_t* server) {
  uv_handle_t* handle = (uv_handle_t*) malloc(sizeof *handle);

  if (uv_accept(server, handle, on_close, NULL)) {
    FATAL("uv_accept failed");
  }

  uv_read_start(handle, after_read);
}


static void on_server_close(uv_handle_t* handle, int status) {
  ASSERT(handle == &server);
  ASSERT(status == 0);
}


static int echo_start(int port) {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", port);
  int r;

  r = uv_tcp_init(&server, on_server_close, NULL);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Socket creation error\n");
    return 1;
  }

  r = uv_bind(&server, (struct sockaddr*) &addr);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Bind error\n");
    return 1;
  }

  r = uv_listen(&server, 128, on_accept);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Listen error\n");
    return 1;
  }

  return 0;
}


static int echo_stop() {
  return uv_close(&server);
}


static uv_buf echo_alloc(uv_handle_t* handle, size_t suggested_size) {
  uv_buf buf;
  buf.base = (char*) malloc(suggested_size);
  buf.len = suggested_size;
  return buf;
}


HELPER_IMPL(echo_server) {
  uv_init(echo_alloc);
  if (echo_start(TEST_PORT))
    return 1;

  fprintf(stderr, "Listening!\n");
  uv_run();
  return 0;
}

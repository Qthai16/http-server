# Suggested Extensions

Ordered by implementation effort (ascending).

---

## 1. Graceful Shutdown
**Complexity: Low–Medium | Files: `SimpleServer.cpp` (stop), `SimpleServer.h`**

Currently `stop()` signals all workers immediately with no drain phase. In-flight
handler tasks complete (because `taskPool_->stop(true)` drains the pool) but any
connection in the middle of reading its request body is cut off, and partially-
written responses are abandoned.

Add a tri-state shutdown: `RUNNING → DRAINING → STOPPED`.
In `DRAINING`, stop accepting new connections but let the acceptor loop exit, allow
all IO workers to finish their current read/write cycles, and only stop IO workers
once `activeConn` reaches 0 (or a timeout expires).

---

## 2. Keep-Alive / Connection Reuse (HTTP/1.1)
**Complexity: Medium | Files: `HttpMessage.cpp`, `SimpleServer.cpp` (handleWrite, cleanupEvent), `SimpleServer.h` (ConnData)**

The response path already calls `resetData()` and re-arms `EPOLLIN` after a
completed write, which is structurally correct for keep-alive. What's missing:

- Parse the `Connection: keep-alive / close` header in `parse_headers` and store
  a `bool keepAlive` flag on `ConnData`.
- In `handleWrite`, after `write_done()`, check `keepAlive`: if false, call
  `cleanupEvent` instead of re-arming.
- Add an idle timestamp to `ConnData` and enforce a per-connection read timeout
  (see below) to bound resource usage from idle keep-alive connections.

---

## 3. Idle Connection Timeout (Timer Wheel)
**Complexity: Medium–High | Files: `SimpleServer.h` (ConnData, IOWorker), `SimpleServer.cpp` (eventLoop)**

Currently `epoll_wait` is called with timeout=-1 (wait forever). Idle connections
(client connected but not sending) hold a `ConnData` and an fd indefinitely.

Replace `-1` with a short timeout (e.g. 1000 ms). On each `epoll_wait` return,
walk a min-heap of `(expiry_time, ConnData*)` sorted by `last_active` timestamp.
Evict and `cleanupEvent` any connection whose deadline has passed.
`ConnData` needs a `std::chrono::steady_clock::time_point last_active` field;
update it on every `handleRead`/`handleWrite`.

---

## 4. Chunked Transfer Encoding
**Complexity: Medium | Files: `HttpMessage.h` (new ReadType), `HttpMessage.cpp` (write_header, write_file_body, write_mem_body)**

For streaming responses (large files, SSE, server-push) a fixed `Content-Length`
requires knowing the full size upfront and buffers the entire body. Chunked
transfer emits `Transfer-Encoding: chunked` in the header and wraps each
`write_file_body` / `write_mem_body` call in a `<hex-size>\r\n<data>\r\n` chunk
frame, terminated by `0\r\n\r\n`.

Add a `CHUNKED_READ` variant to `ReadType`. `write_header` omits `Content-Length`
and writes `Transfer-Encoding: chunked` instead. Each body-write call prepends the
chunk size line and appends `\r\n`. `write_done` returns true only after the
terminator chunk is flushed.

---

## 5. Per-IP Rate Limiting / Backpressure
**Complexity: Medium | Files: `SimpleServer.h` (add per-IP map to Acceptor or IOWorker), `SimpleServer.cpp` (Acceptor::eventLoop)**

The acceptor currently distributes every new connection immediately. Under a SYN
flood or aggressive client, a single IP can exhaust `activeConn` and starve
legitimate clients.

In the Acceptor, maintain a `std::unordered_map<uint32_t, int> connPerIP` (keyed
by IPv4 address). Before calling `addConnection`, check whether the IP is already
at its per-IP limit; if so, immediately `::close(clientfd)` and increment a
`dropConn` counter. The map entry decrements in `cleanupEvent` (requires passing
the IP back, e.g. via `ConnData::addr`).

---

## 6. HTTP Request Pipelining
**Complexity: High | Files: `HttpMessage.cpp` (parse_request, resetData), `SimpleServer.cpp` (handleWrite)**

A pipelined HTTP/1.1 client sends the next request before the previous response
is fully sent. Currently `resetData()` calls `recv_buf_.resetBuf()`, discarding
any bytes already received for the next request.

Fix: after writing a complete response, check whether `recv_buf_` contains
unconsumed bytes (i.e. `rd_avail() > 0` after the previous request's body was
consumed). If so, preserve them and immediately re-enter `parse_request` for the
next request. Requires tracking how many bytes of `recv_buf_` belong to the
current request vs the next, and ordering responses to match request order.

---

## 7. TLS via OpenSSL (build flag already present)
**Complexity: High | Files: `SimpleServer.h` (ConnData — add `SSL*`, handshake state), `SimpleServer.cpp` (handleRead, handleWrite, addConn), `CMakeLists.txt` (USE_OPENSSL already wired)**

The build system already has `USE_OPENSSL` and `libs/ssl/SSLUtils.cpp`. What's
missing is the non-blocking TLS layer in the IO path.

Each `ConnData` needs an `SSL*` field and a handshake state
(`HANDSHAKE / ESTABLISHED`). When a new connection arrives, arm `EPOLLIN` and
call `SSL_do_handshake`; on `SSL_ERROR_WANT_READ/WRITE` re-arm the appropriate
epoll edge instead of blocking. Once established, replace `::recv`/`::send` with
`SSL_read`/`SSL_write` with the same `WANT_READ/WRITE` re-arm pattern.
`cleanupEvent` calls `SSL_shutdown` then `SSL_free` before `::close`.

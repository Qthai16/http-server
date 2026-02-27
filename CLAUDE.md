# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Debug build (with AddressSanitizer, OpenSSL, and tests)
./build_local.sh --build_type=Debug --src_path=. --targets=all

# Manual CMake build (Release, no OpenSSL, no tests)
mkdir build && cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DUSE_OPENSSL=OFF -DBUILD_TEST=OFF ..
ninja

# Manual CMake build (Release, with OpenSSL and tests)
mkdir build-Release
cmake -G "Unix Makefiles" -B build-Release -DCMAKE_BUILD_TYPE=Release -DUSE_OPENSSL=ON -DBUILD_TEST=ON .
cmake --build build-Release
```

CMake options: `USE_OPENSSL`, `USE_LIBCURL`, `USE_BOOST`, `BUILD_TEST`, `BUILD_EXP` (all OFF by default).

## Run Tests

```bash
# All tests
cd build && ctest --output-on-failure -j 4

# By label
ctest -L unit
ctest -L integrate

# Run the server (default port 11225, 4 IO worker threads)
./http-server 0.0.0.0:11225 4
```

## Architecture

This is a non-blocking, event-driven HTTP/1.x server built on Linux epoll with a multi-threaded architecture:

```
Acceptor Thread → round-robin → IO Worker Threads (epoll) → Handler Thread Pool
```

- **Acceptor**: Single thread accepts new TCP connections and distributes them to IO workers.
- **IO Workers**: Each has its own epoll instance; reads incoming requests and writes responses back to sockets. The number of IO workers is set via `opts.workerSize`.
- **Handler Pool** (`libs/WorkerPool.h`): `NotifyQueueWorker<T>` — a generic thread pool that processes `HandlerTask` jobs dispatched by IO workers.

### Key Source Files

| File | Role |
|------|------|
| `src/SimpleServer.h/.cpp` | Core server: `Acceptor`, `IOWorker`, `EpollHandle`, `ConnData`, `SimpleServer` |
| `src/HttpMessage.h/.cpp` | HTTP parsing (`HTTPRequest`) and response building (`HTTPResponse`) |
| `test.cpp` | Main entry point; registers route handlers and starts the server |
| `libs/WorkerPool.h` | Generic thread pool (`NotifyQueueWorker<T>`) |
| `libs/MemBuffer.h` | Dynamic memory buffer (`MemBuf`) used by request/response I/O |
| `libs/StrUtils.h` | String formatting and utilities |
| `libs/FileUtils.h` | File I/O and MIME type detection |

### Request Lifecycle

1. Acceptor accepts connection → creates `ConnData` → assigns to an IO worker.
2. IO worker's epoll detects `EPOLLIN` → reads bytes into request buffer.
3. `HTTPRequest::parse_request()` parses the complete HTTP request.
4. IO worker creates a `HandlerTask` and enqueues it to the handler thread pool.
5. Handler thread calls the registered route handler `void(HTTPRequest*, HTTPResponse*)`.
6. IO worker detects `EPOLLOUT` → writes serialized `HTTPResponse` to the socket.

### Registering Handlers

```cpp
server_->addHandlers({
    {"/path", {HTTPMethod::GET, [](HTTPRequest *req, HTTPResponse *res) {
        res->status_code(HTTPStatusCode::OK);
        res->str_body("Hello");
        res->insert_header({"Content-Type", "text/plain"});
    }}},
});
```

Route keys can be plain paths or regex strings. `file_body()` streams a file directly.

### Threading Model Notes

- Statistics use atomic operations; job queues use mutex + condition variable.
- `CACHE_CONN` compile flag enables optional connection pooling/reuse.
- Debug builds enable AddressSanitizer; don't mix with thread sanitizer.

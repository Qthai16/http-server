### Features
- Parse HTTP request and response messages, URL query parameters, and headers
- Non-blocking socket with epoll and a multi-threaded handler pool for high-throughput I/O
- Regex-based route matching; supports GET and POST handlers
- Serve static HTML/CSS files with auto-detected MIME types
- Optional OpenSSL (TLS) and LMDB support via CMake flags
- Benchmark-friendly: tested with Apache Benchmark (`ab`) and `wrk`

### Project Layout

```
.
├── libs/
│   ├── http/           # Core HTTP library (HttpMessage, SimpleServer)
│   ├── ssl/            # SSLUtils (OpenSSL wrapper)
│   ├── lmdb/           # LMDBWrapper (key-value store helper)
│   ├── MemBuffer.h     # Dynamic byte buffer
│   ├── FixedQueue.h    # Lock-free fixed-size queue
│   ├── WorkerPool.h    # Generic thread pool
│   ├── FileUtils.h     # File I/O and MIME detection
│   └── StrUtils.h      # String formatting utilities
├── exp/                # Experimental: io_uring and coroutine prototypes
├── test/               # Unit and integration tests (GoogleTest)
├── test.cpp            # Main entry point; registers route handlers
└── CMakeLists.txt
```

### Architecture

Non-blocking, event-driven HTTP/1.x server built on Linux epoll:

```
Acceptor Thread → round-robin → IO Worker Threads (epoll) → Handler Thread Pool
```

- **Acceptor**: accepts new TCP connections and distributes them to IO workers.
- **IO Workers**: each has its own epoll instance; reads requests and writes responses.
- **Handler Pool** (`libs/WorkerPool.h`): generic `NotifyQueueWorker<T>` thread pool that executes route handlers dispatched by IO workers.

### Build

Requirements: `g++`, `cmake`, `ninja-build` (or `make`), `libssl-dev`, `liblmdb-dev`.

```sh
# Release build (Ninja, no tests)
mkdir build && cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DUSE_OPENSSL=OFF -DBUILD_TEST=OFF ..
ninja

# Release build with OpenSSL and tests
mkdir build-Release
cmake -G "Unix Makefiles" -B build-Release -DCMAKE_BUILD_TYPE=Release -DUSE_OPENSSL=ON -DBUILD_TEST=ON .
cmake --build build-Release

# Debug build with AddressSanitizer (uses local build script)
./build_local.sh --build_type=Debug --src_path=. --targets=all
```

CMake options: `USE_OPENSSL`, `USE_LMDB`, `USE_LIBCURL`, `USE_BOOST`, `BUILD_TEST`, `BUILD_EXP` (defaults: `USE_OPENSSL=ON`, `USE_LMDB=ON`, others `OFF`).

### Run

```sh
# Default: 0.0.0.0:11225, 4 IO worker threads
./http-server 0.0.0.0:11225 4
```

### Tests

```sh
cd build-Release
# All tests
ctest --output-on-failure -j 4

# By label
ctest -L unit
ctest -L integrate
```

### Benchmark

```sh
ulimit -n 10000 && ./http-server 0.0.0.0:11225 4
ulimit -n 10000 && ./benchmark.sh 2>&1 | tee benchmark-result.txt
```

### Static Analysis

Prerequisites: `clang-tidy`, `run-clang-tidy`

```sh
run-clang-tidy -clang-tidy-binary /usr/bin/clang-tidy -p <build_dir> -quiet
```

### Registering Route Handlers

```cpp
using namespace libs::http;

server_->addHandlers({
    {"/hello", {HTTPMethod::GET, [](HTTPRequest *req, HTTPResponse *res) {
        res->status_code(HTTPStatusCode::OK);
        res->str_body("Hello, World!");
        res->insert_header({"Content-Type", "text/plain"});
    }}},
});
```

Route keys can be plain paths or regex strings. Use `res->file_body(path)` to stream a file.

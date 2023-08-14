### Features
- Parse HTTP request and response message, URL query parameters and headers
- Use non blocking socket epoll system call and a simple threadpool to optimize read/write time on socket
- Support basic methods for server: set address, set port, set thread pool size, add function handler, set response status code
- Serve several demo html pages and css file
- Use Apache benchmarking tool and wrk for benchmarking the demo server
- Auto detect MIME type based on file extension
- Support 2 methods GET and POST for demo

#### Build
```sh
    cd <project_src> && mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j 4
```
#### Run
```sh
    [executable_path] <address> <port>
```
#### Benchmark test
```sh
   ./benchmark.sh 2>&1 | tee benchmark-result.txt
```

add remote endpoint address to event data
support "Transfer-Encoding: chunked"
support "Content-Type: multipart/form-data"
parse accept header (comma delimeter)
test if correctly parse query param
log request to file
refer simple-web-server API
json11 submodule?
set callback for specific events: close connection, on open socket

### Build project with clang
### build as dynamic lib and use dlopen to load it when test run
### Write simple test/unit test with google test or create own macro
#### openssl: symmetric AES256-CFB128 encryption
#### base64 encode/decode
#### save private key/public key in source code using runtime xor/compile xor
#### TODO: implement HTTP multipart/form-data
#### TODO: implement websocket upgrading
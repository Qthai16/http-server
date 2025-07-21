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
    cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DUSE_OPENSSL=OFF -DUSE_LIBCURL=OFF -DUSE_BOOST=OFF -DBUILD_TEST=OFF ..
    ninja
```
#### Run
```sh
    [executable_path] <address> <port>
```

#### Library test
- Run all test: 
```sh
    ctest --output-on-failure -j 4
```
- Run unit test or integration test only, add flag "-L unit" or "-L intergrate"

#### Benchmark test
```sh
    ulimit -n 10000 && ./http-server 0.0.0.0 11225
    ulimit -n 10000 && ./benchmark.sh 2>&1 | tee benchmark-result.txt
```

#### Run static analysis
Prequesites: install run-clang-tidy and clang-tidy
```sh
run-clang-tidy -clang-tidy-binary /usr/bin/clang-tidy -p <build_dir> -quiet
```

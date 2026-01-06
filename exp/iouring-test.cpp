#include <iostream>
#include <string>
#include <fstream>
#include <sys/stat.h>

#include "libs/IOUringDummy.h"
#include "libs/FileUtils.h"
#include "libs/Defines.h"
#include "libs/StrUtils.h"

#include <string_view>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <vector>
#include <thread>

// sync read vs io uring read

// eg1: open a file, read all data from it
// eg2: open a file, read all data, write to another file
// eg3: provide_buffer to io_uring
template<typename CharT>
bool icase_compare(std::basic_string_view<CharT> a, const std::basic_string_view<CharT> b) {
    std::cout << "view tmpl" << std::endl;
    return a.length() == b.length() &&
           std::equal(a.begin(), a.end(), b.begin(),
                      [](CharT c1, CharT c2) {
                          return std::tolower(c1) == std::tolower(c2);
                      });
}

// , [&readFailed](size_t ret) {
//     if (expr_unlikely(ret < 0)) {
//         fprintf(stderr, "readv failed: %ld\n", ret);
//         readFailed++;
//     }
// }

// [&writeFailed](size_t ret) {
//     if (expr_unlikely(ret < 0)) {
//         fprintf(stderr, "writev failed: %ld\n", ret);
//         writeFailed++;
//     }
// }

int sync_read_write_file(std::string infile, std::string outfile, int64_t blksize) {
    int fd = ::open(infile.c_str(), O_RDONLY, 0644);
    if (fd < 0) return -1;
    DEFER(::close(fd));
    struct ::stat st;
    if (::stat(infile.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
        return -1;
    int outfd = ::open(outfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, st.st_mode & 0777);
    DEFER(::close(outfd));
    const int64_t filesize = st.st_size;
    off_t offt = 0;
    auto cursize = filesize;

    std::string buffer;
    buffer.resize(filesize);

    std::cout << "sync copy file" << std::endl;
    while (cursize > 0) {
        auto chunksize = std::min(blksize, cursize);
        libs::read(fd, offt, &buffer.at(offt), chunksize);
        cursize -= chunksize;
        offt += chunksize;
    }
    libs::write(outfd, 0, buffer.data(), buffer.size());
    return 0;
}

int uring_batch_read_write(std::string infile, std::string outfile, int64_t blksize) {
    int fd = ::open(infile.c_str(), O_RDONLY, 0644);
    if (fd < 0) return -1;
    DEFER(::close(fd));
    struct ::stat st;
    if (::stat(infile.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
        return -1;
    int outfd = ::open(outfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, st.st_mode & 0777);
    DEFER(::close(outfd));
    const int64_t filesize = st.st_size;
    off_t offt = 0;
    auto cursize = filesize;

    std::string buffer;
    buffer.resize(filesize);

    thread_local libs::IOUringDummy uringDummy(1024);
    // int readFailed{0}, writeFailed{0};
    while (cursize > 0) {
        auto chunksize = std::min(blksize, cursize);
        uringDummy.async_readv(fd, offt, &buffer.at(offt), chunksize);
        cursize -= chunksize;
        offt += chunksize;
    }
    uringDummy.async_writev(outfd, 0, buffer.data(), buffer.size());
    uringDummy.submit_and_wait();
    return 0;
    // printf("thread: %d, failed read/write: %d/%d\n", std::this_thread::get_id(), readFailed, writeFailed);
}

int uring_read_write(std::string infile, std::string outfile, int64_t blksize) {
    int fd = ::open(infile.c_str(), O_RDONLY, 0644);
    if (fd < 0) return -1;
    DEFER(::close(fd));
    struct ::stat st;
    if (::stat(infile.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
        return -1;
    int outfd = ::open(outfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, st.st_mode & 0777);
    DEFER(::close(outfd));
    const int64_t filesize = st.st_size;
    off_t offt = 0;
    auto cursize = filesize;

    std::string buffer;
    buffer.resize(filesize);

    thread_local libs::IOUringDummy uringDummy(1024);
    int readFailed{0}, writeFailed{0};

    while (cursize > 0) {
        auto chunksize = std::min(blksize, cursize);
        uringDummy.readv(fd, offt, &buffer.at(offt), chunksize);
        cursize -= chunksize;
        offt += chunksize;
    }
    uringDummy.writev(outfd, 0, buffer.data(), buffer.size());
    return 0;
}

template<typename Str>
bool icase_compare(const Str &a, const Str &b) {
    std::cout << "str tmpl" << std::endl;
    return icase_compare<typename Str::value_type>(a, b);
}

#include <future>

int main(int argc, const char *argv[]) {
    // std::async([]() {
    //     std::cout << "sync printed" << std::endl;
    // });
    std::vector<std::future<std::thread::id>> futureList;
    std::vector<std::thread> ths;
    for (auto i = 0; i < 10; i++) {
        // auto f = std::async(std::launch::async, [i]() {
        std::promise<std::thread::id> prom;
        auto f = prom.get_future();
        ths.emplace_back([i](std::promise<std::thread::id> &&pr) {
            std::cout << libs::simple_format("{}: in thread: {}", i, std::this_thread::get_id()) << std::endl;
            pr.set_value(std::this_thread::get_id());
        },
                         std::move(prom));
        futureList.push_back(std::move(f));
    }
    for (auto &f: futureList) {
        f.get();
    }
    for (auto &t: ths) {
        t.join();
    }
    return 0;

    if (argc < 3) {
        std::cerr << "./uring-test [in_file] [opt:sync/uring]" << std::endl;
        return 1;
    }
    auto availOpts = {"raw", "uring", "splice"};
    auto it = std::find_if(availOpts.begin(), availOpts.end(), [s = argv[2]](const char *v) -> bool {
        return strncmp(s, v, std::min(strlen(s), strlen(v))) == 0;
    });
    if (it == availOpts.end()) {
        std::cerr << "unknown option: '" << argv[2] << "'" << std::endl;
        return 1;
    }
    std::string filename(argv[1]);
    std::string opt(argv[2]);
    std::string cloneFileName = filename;
    if (strcmp(argv[2], "raw") == 0) {
        cloneFileName += ".raw";
    } else if (strcmp(argv[2], "uring") == 0) {
        cloneFileName += ".uring";
    } else if (strcmp(argv[2], "splice") == 0) {
        cloneFileName += ".splice";
    }

    const int64_t blksize = 4096;
    bool batchIOReq = true;
    if (opt == "raw") {
        std::cout << "sync copy file" << std::endl;
        return sync_read_write_file(filename, cloneFileName, blksize);
    } else if (opt == "uring") {
        std::cout << "io_uring copy file" << std::endl;
        std::vector<std::thread> workers;
        for (auto i = 0; i < std::thread::hardware_concurrency(); i++) {
            if (batchIOReq) {
                workers.emplace_back([&]() -> int {
                    std::string outfile = libs::simple_format("{}.{}", cloneFileName, std::this_thread::get_id());
                    return uring_batch_read_write(filename, outfile, blksize);
                });
            } else {
                workers.emplace_back([&]() -> int {
                    std::string outfile = libs::simple_format("{}.{}", cloneFileName, std::this_thread::get_id());
                    return uring_read_write(filename, outfile, blksize);
                });
            }
        }
        for (auto &t: workers) {
            t.join();
        }
        workers.clear();
        return 0;
    } else if (opt == "splice") {
        // std::cout << "splice copy file" << std::endl;
        // int pfd[2];
        // if (::pipe(pfd) != 0) {
        //     std::cerr << "pipe failed: " << errno << std::endl;
        //     return 1;
        // }
        // DEFER(close(pfd[0]));
        // DEFER(close(pfd[1]));
        // while (cursize > 0) {
        //     auto chunksize = std::min(blkSize, cursize);
        //     ::splice(fd, NULL, pfd[1], NULL, chunksize, 0);
        //     ::splice(pfd[0], NULL, outfd, NULL, chunksize, 0);
        //     cursize -= chunksize;
        // }
    }
    return 0;
}

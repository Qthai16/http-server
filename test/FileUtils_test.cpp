#include <iostream>
#include <string>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtest/gtest.h>

#include "libs/FileUtils.h"

#define OPEN_PERM 00644

using namespace libs;

TEST(file_utils, is_file) {
    std::ofstream f("./abc.txt");
    f.close();
    ASSERT_TRUE(isFile("abc.txt"));
}

TEST(file_utils, is_dir) {
    auto rc = makeDir("xyz");
    switch (rc) {
        case 0:
            break;
        case -1: {
            ASSERT_EQ(errno, EEXIST);
        } break;
        default:
            ASSERT_TRUE(false);
    }
    ASSERT_TRUE(isDir("xyz"));
}

TEST(file_utils, read_write) {
    std::string data("this is a text line\n");
    {
        std::ofstream in("in.txt", std::ios::trunc);
        in << data;
        in.close();
    }
    auto infile = "in.txt";
    auto outfile = "out.txt";
    int32_t oflags = O_RDWR | O_CREAT;
    auto fd = ::open(infile, O_RDONLY, OPEN_PERM);
    auto fd2 = ::open(outfile, O_RDWR | O_CREAT | O_TRUNC, OPEN_PERM);
    ASSERT_TRUE(fd > 0 && fd2 > 0);
    auto inCloser = Defer([fd]() { ::close(fd); });
    auto outCloser = Defer([fd2]() { ::close(fd2); });
    std::string readBuf;
    struct ::stat st;
    ASSERT_EQ(::fstat(fd, &st), 0);
    if (st.st_size > 0) readBuf.resize(st.st_size);
    printf("read data from %s\n", infile);
    ASSERT_TRUE(libs::read(fd, 0, readBuf.data(), readBuf.size()));
    ASSERT_EQ(readBuf, data);
    ASSERT_TRUE(libs::write(fd2, 0, readBuf.data(), readBuf.size()));
    std::string readBuf2(readBuf.size(), '\0');
    ASSERT_TRUE(libs::read(fd2, 0, readBuf2.data(), readBuf2.size()));
    ASSERT_EQ(readBuf2, readBuf);
}

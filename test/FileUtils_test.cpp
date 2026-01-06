#include <iostream>
#include <string>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtest/gtest.h>

#include "libs/FileUtils.h"
#include "libs/Defines.h"

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
    ASSERT_EQ(libs::read(fd, 0, readBuf.data(), readBuf.size()), readBuf.size());
    ASSERT_EQ(readBuf, data);
    ASSERT_EQ(libs::write(fd2, 0, readBuf.data(), readBuf.size()), readBuf.size());
    std::string readBuf2(readBuf.size(), '\0');
    ASSERT_EQ(libs::read(fd2, 0, readBuf2.data(), readBuf2.size()), readBuf2.size());
    ASSERT_EQ(readBuf2, readBuf);
}

TEST(file_utils, file_size) {
    {
        std::string data("this is a text line\n");
        std::ofstream in("textfile.txt", std::ios::out | std::ios::trunc);
        in << data;
        in.close();
        EXPECT_EQ(libs::file_size("textfile.txt"), data.size());
    }
    {
        std::string data{0x38, 0x12,0x36,0x1f,0x27,0x12,0x11,0x63,0x24, 0x17, 0x2c, 0x2d, 0x4e};
        std::ofstream in("binfile.txt", std::ios::out | std::ios::trunc | std::ios::binary);
        in << data;
        in.close();
        EXPECT_EQ(libs::file_size("binfile.txt"), data.size());
    }
}

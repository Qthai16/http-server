#include <iostream>
#include <string>
#include <fstream>

#include <gtest/gtest.h>

#include "libs/FileUtils.h"

TEST(file_utils, is_file) {
    std::ofstream f("./abc.txt");
    f.close();
    ASSERT_TRUE(libs::isFile("abc.txt"));
}

TEST(file_utils, is_dir) {
    auto rc = libs::makeDir("xyz");
    switch(rc) {
        case 0: break;
        case -1: {
            ASSERT_EQ(errno, EEXIST);
        } break;
        default: ASSERT_TRUE(false);
    }
    ASSERT_TRUE(libs::isDir("xyz"));
}


// todo: test_read
// todo: test_write
#include <gtest/gtest.h>
#include <string>

#include "sample_codes/RawString.h"

TEST(RawStr, tc1) {
    // Default constructor
    RawStr s1;
    ASSERT_EQ(s1.data(), nullptr);
    ASSERT_EQ(s1.size(), 0);
    ASSERT_EQ(s1.capacity(), 0);
    ASSERT_EQ(s1.c_str(), "");

    // append
    s1.append("hello");
    ASSERT_TRUE(s1.data() != nullptr);
    ASSERT_EQ(s1.size(), strlen("hello"));
    ASSERT_STREQ(s1.c_str(), "hello");
    s1 += " world";
    ASSERT_EQ(s1.size(), strlen("hello world"));
    ASSERT_STREQ(s1.c_str(), "hello world");
    s1.append(std::string(1024, 'a').c_str(), 1024);
    ASSERT_EQ(s1.size(), strlen("hello world") + 1024);
    std::string expected;
    expected += "hello world";
    expected += std::string(1024, 'a');
    ASSERT_STREQ(s1.c_str(), expected.c_str());
    
    // copy constructor
    RawStr s2(s1);
    ASSERT_TRUE(s2.data() != nullptr);
    ASSERT_EQ(s2.size(), s1.size());
    ASSERT_STREQ(s2.c_str(), s1.c_str());
    ASSERT_EQ(s1, s2);

    // copy assignment
    RawStr s100;
    s100 += "this is a string value\n";
    RawStr s101("this is another string");
    s100 = s101;
    ASSERT_EQ(s100, s101);

    ASSERT_EQ(RawStr{}, RawStr{});

    auto s1mem = s1.data();
    auto s1size = s1.size();
    auto s1cap = s1.capacity();
    RawStr s1cp(s1);
    // move constructor
    RawStr s3(std::move(s1));
    ASSERT_EQ(s1.data(), nullptr);
    ASSERT_EQ(s1.size(), 0);
    ASSERT_EQ(s1.capacity(), 0);
    ASSERT_EQ(s3.data(), s1mem);
    ASSERT_EQ(s3.size(), s1size);
    ASSERT_EQ(s3.capacity(), s1cap);
    ASSERT_EQ(s3, s1cp);

    // move assignment
    RawStr s5;
    s5.append("abslfhjasjflsjadflsjlf");
    s5 = std::move(s3);
    ASSERT_EQ(s5.data(), s1mem);
    ASSERT_EQ(s5.size(), s1size);
    ASSERT_EQ(s5.capacity(), s1cap);
    ASSERT_EQ(s5, s1cp);
}

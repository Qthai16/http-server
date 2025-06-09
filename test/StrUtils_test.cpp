#include <iostream>
#include <string>

#include <gtest/gtest.h>

#include "libs/StrUtils.h"

enum TestEnum1 {
    A = 1,
    B = 2,
};
enum TestEnum2 {
    X = 100,
    Y = 101,
    Z = 102,
};

static std::ostream &operator<<(std::ostream &os, const TestEnum2 &e) {
    switch (e) {
        case TestEnum2::X:
            os << "X";
            break;
        case TestEnum2::Y:
            os << "Y";
            break;
        case TestEnum2::Z:
            os << "Z";
            break;
        default:
            os << "Unknown";
            break;
    }
    return os;
}

TEST(str_utils, simple_format) {
    using namespace libs;
    ASSERT_EQ(simple_format("{} normal case: int: {}, double: {}, str: {}", "Start", 1000000, 124.23, std::string{"abcxyz"}),
              sprintf_format("%s normal case: int: %d, double: %f, str: %s", "Start", 1000000, 124.23, "abcxyz"));
    ASSERT_EQ(simple_format("More args than placeholder: {}, {}", "abc", 22.34f, "alfkjalfd"),
              sprintf_format("More args than placeholder: %s, %f", "abc", 22.34f, "alfkjalfd"));
    // ASSERT_EQ("More placeholder than argument: {}, {}", "abc");
    ASSERT_EQ(simple_format("No placeholder", 124, 22.34, 32, "alfkjalfd"), "No placeholder");
    ASSERT_EQ(simple_format("No placeholder, no args"), "No placeholder, no args");
    ASSERT_EQ(simple_format("{{}} Escape placeholder {{}} case: {}, {{}}", 999, 124.23), "{} Escape placeholder {} case: 999, {}");

    ASSERT_EQ(simple_format("{{} string is {{} and {}}, {{}", "Test", 16, 17.02, "end"),
              sprintf_format("{%s string is {%d and %f}, {%s", "Test", 16, 17.02, "end"));
    ASSERT_EQ(simple_format("{{Test {{abc that contain: {{", "not_used", "not_used"), "{{Test {{abc that contain: {{");
    ASSERT_EQ(simple_format("{asdfgh{asdfgh{", "not_used", "not_used"), "{asdfgh{asdfgh{");
    ASSERT_EQ(simple_format("abc{{}}abc", 123), "abc{}abc");
    ASSERT_EQ(simple_format("{{{{{{{{{{{{{{{{{{{{{{{{"), "{{{{{{{{{{{{{{{{{{{{{{{{");
    ASSERT_EQ(simple_format("{{{{{{{{{{{{{{{{{{{{{{{{", 123), "{{{{{{{{{{{{{{{{{{{{{{{{");
    ASSERT_EQ(simple_format("Test string that contain: {{}", 100, 1000), "Test string that contain: {100");
    ASSERT_EQ(simple_format("vector<int>: {}", std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9}), "vector<int>: [1, 2, 3, 4, 5, 6, 7, 8, 9]");
    ASSERT_EQ(simple_format("set<double>: {}", std::set<double>{1.2, 2.3, 3.4, 4.5}),
              sprintf_format("set<double>: (%f, %f, %f, %f)", 1.2, 2.3, 3.4, 4.5));
    ASSERT_EQ(simple_format("map<string, string>: {}",
                            std::map<std::string, std::string>{{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}}),
              "map<string, string>: {key1: value1, key2: value2, key3: value3}");
    ASSERT_EQ(simple_format("enum with no stream operator: {}, {}", TestEnum1::A, TestEnum1::B), "enum with no stream operator: 1, 2");
    ASSERT_EQ(simple_format("enum with stream operator: {}, {}", TestEnum2::X, TestEnum2::Y), "enum with stream operator: X, Y");
}

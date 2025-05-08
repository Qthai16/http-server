/*
 * File:   StrUtils.h
 * Author: thaipq
 *
 * Created on Tue Apr 29 2025 10:49:40 AM
 */

#ifndef LIBS_STRUTILS_H
#define LIBS_STRUTILS_H

#include <string>
#include <sstream>
#include <cstdio>
#include <set>
#include <vector>
#include <map>
#include <algorithm>
#include <cassert>
#include <cstring>

#include "Defines.h"

namespace libs {
    inline std::string toHexStr(const char *buf, std::size_t size) {
        if (expr_unlikely(!buf || !size))
            return {};
        std::size_t cnt = 0;
        char numbuf[32];
        std::stringstream ss;
        while (size-- > 0) {
            // if (cnt++ > 0)
            //     ss.put(' ');
            std::sprintf(numbuf, "%02x", *(unsigned char *) buf);
            ss << numbuf;
            buf++;
        }
        return ss.str();
    }

    #if __cplusplus >= 201703L
    inline std::string toHexStr(std::string_view str) {
        return toHexStr(str.data(), str.size());
    }
    #endif

    inline void to_lower(std::string &s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    }

    inline void to_upper(std::string &s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    }

    inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }));
        return s;
    }

    inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }).base(),
                s.end());
        return s;
    }

    inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
    }

    namespace detail {
        // refer implementation from CLI11 TypeTools
        enum class enabler {};
        constexpr enabler dummy = {};

        template<typename T>
        using IsNumber = std::enable_if_t<std::is_arithmetic<T>::value, enabler>;
        template<typename T>
        using IsString = std::enable_if_t<std::is_same<std::string, T>::value, enabler>;
        template<typename T>
        using IsEnum = std::enable_if_t<std::is_enum<T>::value, enabler>;
        template<typename T>
        using IsOthers = std::enable_if_t<!std::is_arithmetic<T>::value && !std::is_same<std::string, T>::value && !std::is_enum<T>::value, enabler>;
        struct Writer {
            template<typename T, IsNumber<T> = dummy>
            static std::string to_string(const T &v) {
                return std::to_string(v);
            }

            template<typename T, IsString<T> = dummy>
            static std::string to_string(const T &v) {
                return v;
            }

            static std::string to_string(const char *v) {
                return v;
            }

            template<typename T, IsEnum<T> = dummy>
            static std::string to_string(const T &v) {
                std::ostringstream os;
                os << v;
                return os.str();
            }

            template<typename T, IsOthers<T> = dummy>
            static std::string to_string(const T &v) {
                std::ostringstream os;
                os << v;
                return os.str();
            }

            template<typename K, typename V>
            static std::string to_string(const typename std::pair<K, V> &v) {
                std::ostringstream o;
                o << to_string(v.first) << ": " << to_string(v.second);
                return o.str();
            }

            template<typename T>
            static std::string to_string(const T &beg, const T &end) {
                std::ostringstream o;
                for (T it = beg; it != end; ++it) {
                    if (it != beg)
                        o << ", ";
                    o << to_string(*it);
                }
                return o.str();
            }

            template<typename T>
            static std::string to_string(const std::vector<T> &t) {
                std::ostringstream o;
                o << "[" << to_string(t.begin(), t.end()) << "]";
                return o.str();
            }

            template<typename K, typename V>
            static std::string to_string(const std::map<K, V> &m) {
                std::ostringstream o;
                o << "{" << to_string(m.begin(), m.end()) << "}";
                return o.str();
            }

            template<typename T>
            static std::string to_string(const std::set<T> &s) {
                std::ostringstream o;
                o << "{" << to_string(s.begin(), s.end()) << "}";
                return o.str();
            }
        };

        static void format_impl(std::ostream &os, const char *format, std::size_t size) {
            os.write(format, size);
        }

        template<typename T>
        void format_impl(std::ostream &os, const char *format, std::size_t size, const T &arg) {
            if (expr_unlikely(size == 0)) return;
            auto pos = strstr(format, "{");
            if (pos != nullptr && (pos - format + 1) < size) {
                os.write(format, pos - format);
                auto preBytes = pos - format;
                if (*(pos + 1) == '}') {// {}
                    os << detail::Writer::to_string(arg);
                    format_impl(os, pos + 2, size - 2 - preBytes);
                } else if (*(pos + 1) == '{') {
                    if ((pos - format + 2 < size) && (*(pos + 2) == '}')) {
                        if ((pos - format + 3 < size) && (*(pos + 3) == '}')) {// {{}}
                            os << "{}";
                            format_impl(os, pos + 4, size - 4 - preBytes, arg);
                        } else {// {{} at end or {{}X
                            os.put('{');
                            os << detail::Writer::to_string(arg);
                            format_impl(os, pos + 3, size - 3 - preBytes);
                        }
                    } else {// {{ at end or {{X
                        os << "{{";
                        format_impl(os, pos + 2, size - 2 - preBytes, arg);
                    }
                } else {// {X
                    os.put('{');
                    format_impl(os, pos + 1, size - 1 - preBytes, arg);
                }
            } else {
                os << format;
            }
        }

        template<typename T, typename... Args>
        void format_impl(std::ostream &os, const char *format, std::size_t size, const T &arg, Args &&...args) {
            // print argument in cases: {}, {{}, {}}
            // print placeholder in case: {{}}
            // print as it is in cases: {{, }}, {, }
            if (expr_unlikely(size == 0)) return;
            auto pos = strstr(format, "{");
            if (pos != nullptr && (pos - format + 1) < size) {
                os.write(format, pos - format);
                auto preBytes = pos - format;
                if (*(pos + 1) == '}') {// {}
                    os << detail::Writer::to_string(arg);
                    format_impl(os, pos + 2, size - 2 - preBytes, std::forward<Args>(args)...);
                } else if (*(pos + 1) == '{') {
                    if ((pos - format + 2 < size) && (*(pos + 2) == '}')) {
                        if ((pos - format + 3 < size) && (*(pos + 3) == '}')) {// {{}}
                            os << "{}";
                            format_impl(os, pos + 4, size - 4 - preBytes, arg, std::forward<Args>(args)...);
                        } else {// {{} at end or {{}X
                            os.put('{');
                            os << detail::Writer::to_string(arg);
                            format_impl(os, pos + 3, size - 3 - preBytes, std::forward<Args>(args)...);
                        }
                    } else {// {{ at end or {{X
                        os << "{{";
                        format_impl(os, pos + 2, size - 2 - preBytes, arg, std::forward<Args>(args)...);
                    }
                } else {// {X
                    os.put('{');
                    format_impl(os, pos + 1, size - 1 - preBytes, arg, std::forward<Args>(args)...);
                }
            } else {
                // no other placeholder found, stop process more args
                os << format;
            }
        }
    }// namespace detail

    template<typename... Args>
    std::string simple_format(const std::string &format, Args &&...args) {
        std::stringstream ss;
        detail::format_impl(ss, format.data(), format.size(), std::forward<Args>(args)...);
        return ss.str();
    }

    template<typename... Args>
    void simple_format(std::ostream &os, const std::string &format, Args &&...args) {
        detail::format_impl(os, format.data(), format.size(), std::forward<Args>(args)...);
    }

    inline std::string simple_format(const std::string &format) {
        std::stringstream ss;
        detail::format_impl(ss, format.data(), format.size());
        return ss.str();
    }

    namespace test {
#define TEST_ASSERT_EQ(first, second) assert((first) == (second))
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
        struct simple_format_test {
            static std::string sprintf_format(const char *format, ...) {
                char buffer[4096]{0};
                va_list args;
                va_start(args, format);
                vsprintf(buffer, format, args);
                va_end(args);
                return (buffer);
            }

            static void run() {
                TEST_ASSERT_EQ(simple_format("{} normal case: int: {}, double: {}, str: {}", "Start", 1000000, 124.23, std::string{"abcxyz"}),
                               sprintf_format("%s normal case: int: %d, double: %f, str: %s", "Start", 1000000, 124.23, "abcxyz"));
                TEST_ASSERT_EQ(simple_format("More args than placeholder: {}, {}", "abc", 22.34f, "alfkjalfd"),
                               sprintf_format("More args than placeholder: %s, %f", "abc", 22.34f, "alfkjalfd"));
                // TEST_ASSERT_EQ("More placeholder than argument: {}, {}", "abc");
                TEST_ASSERT_EQ(simple_format("No placeholder", 124, 22.34, 32, "alfkjalfd"), "No placeholder");
                TEST_ASSERT_EQ(simple_format("No placeholder, no args"), "No placeholder, no args");
                TEST_ASSERT_EQ(simple_format("{{}} Escape placeholder {{}} case: {}, {{}}", 999, 124.23), "{} Escape placeholder {} case: 999, {}");

                TEST_ASSERT_EQ(simple_format("{{} string is {{} and {}}, {{}", "Test", 16, 17.02, "end"),
                               sprintf_format("{%s string is {%d and %f}, {%s", "Test", 16, 17.02, "end"));
                TEST_ASSERT_EQ(simple_format("{{Test {{abc that contain: {{", "not_used", "not_used"), "{{Test {{abc that contain: {{");
                TEST_ASSERT_EQ(simple_format("{asdfgh{asdfgh{", "not_used", "not_used"), "{asdfgh{asdfgh{");
                TEST_ASSERT_EQ(simple_format("abc{{}}abc", 123), "abc{}abc");
                TEST_ASSERT_EQ(simple_format("{{{{{{{{{{{{{{{{{{{{{{{{"), "{{{{{{{{{{{{{{{{{{{{{{{{");
                TEST_ASSERT_EQ(simple_format("{{{{{{{{{{{{{{{{{{{{{{{{", 123), "{{{{{{{{{{{{{{{{{{{{{{{{");
                TEST_ASSERT_EQ(simple_format("Test string that contain: {{}", 100, 1000), "Test string that contain: {100");
                TEST_ASSERT_EQ(simple_format("vector<int>: {}", std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9}), "vector<int>: [1, 2, 3, 4, 5, 6, 7, 8, 9]");
                TEST_ASSERT_EQ(simple_format("set<double>: {}", std::set<double>{1.2, 2.3, 3.4, 4.5}),
                               sprintf_format("set<double>: {%f, %f, %f, %f}", 1.2, 2.3, 3.4, 4.5));
                TEST_ASSERT_EQ(simple_format("map<string, string>: {}",
                                             std::map<std::string, std::string>{{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}}),
                               "map<string, string>: {key1: value1, key2: value2, key3: value3}");
                TEST_ASSERT_EQ(simple_format("enum with no stream operator: {}, {}", TestEnum1::A, TestEnum1::B), "enum with no stream operator: 1, 2");
                TEST_ASSERT_EQ(simple_format("enum with stream operator: {}, {}", TestEnum2::X, TestEnum2::Y), "enum with stream operator: X, Y");
            }
        };
    }// namespace test
}// namespace libs

#endif// LIBS_STRUTILS_H
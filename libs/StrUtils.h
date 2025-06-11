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
#include "TypeTraits.h"

namespace libs {
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

    inline std::string toHexStr(const char *buf, std::size_t size, char prefix = '\0') {
        if (expr_unlikely(!buf || !size))
            return {};
        std::size_t cnt = 0;
        char numbuf[32];
        std::stringstream ss;
        if (prefix != '\0') ss.put(prefix);
        while (size-- > 0) {
            if (cnt++ > 0)
                if (prefix != '\0') ss.put(prefix);
            std::sprintf(numbuf, "%02x", *(unsigned char *) buf);
            ss << numbuf;
            buf++;
        }
        return ss.str();
    }

    inline std::vector<unsigned char> fromHexStr(const char *buf, std::size_t size) {
        if (expr_unlikely(!buf || !size))
            return {};
        std::string strbuf(buf, size);
        to_lower(strbuf);
        std::vector<unsigned char> ret;
        unsigned int num = 0;
        for (auto i = 0; i < size;) {
            if (std::isspace(*(buf + i))) {
                i++;
                continue;
            }
            if (std::sscanf(buf + i, "%2x", &num) != 1) {
                printf("invalid char\n");
                return {};
            }
            ret.push_back(static_cast<unsigned char>(num));
            i += 2;
        }
        return ret;
    }

#if __cplusplus >= 201703L
    inline std::string toHexStr(std::string_view str) {
        return toHexStr(str.data(), str.size());
    }

    inline std::vector<unsigned char> fromHexStr(std::string_view buf) {
        return fromHexStr(buf.data(), buf.size());
    }
#endif

    namespace detail {
        template<typename T, typename Trait>
        struct Writer {
            static std::string to_string(const T &v);
        };

        template<typename T>
        struct Writer<T, std::enable_if_t<is_number<T>::value, enabler>> {
            static std::string to_string(const T &v) {
                return std::to_string(v);
            }
        };

        template<typename T>
        struct Writer<T, std::enable_if_t<is_string<T>::value, enabler>> {
            static std::string to_string(const T &v) {
                return v;
            }
        };

        // todo: traits for string constructible and invokeable
        template<>
        struct Writer<char *, enabler> {
            static std::string to_string(const char *v) {
                return {v};
            }
        };

        template<typename T>
        struct Writer<T, std::enable_if_t<is_enum<T>::value && !is_ostreamable<T>::value, enabler>> {
            static std::string to_string(const T &v) {
                return std::to_string(static_cast<int>(v));
            }
        };

        // pair
        template<typename T>
        struct Writer<T, std::enable_if_t<is_std_pair<T>::value, enabler>> {
            static std::string to_string(const T &v) {
                auto s1 = Writer<typename T::first_type, enabler>::to_string(v.first);
                auto s2 = Writer<typename T::second_type, enabler>::to_string(v.second);
                return s1 + ": " + s2;
            }
        };

        // vector
        template<typename T>
        struct Writer<T, std::enable_if_t<is_std_vector<T>::value, enabler>> {
            static std::string to_string(const T &v) {
                using ElemType = typename T::value_type;
                std::string ret;
                ret.append("[");
                // todo: iterator like support???
                for (auto it = std::begin(v); it != std::end(v); it++) {
                    if (it != std::begin(v))
                        ret.append(", ");
                    ret.append(Writer<ElemType, enabler>::to_string(*it));
                }
                ret.append("]");
                return ret;
            }
        };

        // set
        template<typename T>
        struct Writer<T, std::enable_if_t<is_std_set<T>::value, enabler>> {
            static std::string to_string(const T &v) {
                using ElemType = typename T::value_type;
                std::string ret;
                ret.append("(");
                for (auto it = std::begin(v); it != std::end(v); it++) {
                    if (it != std::begin(v))
                        ret.append(", ");
                    ret.append(Writer<ElemType, enabler>::to_string(*it));
                }
                ret.append(")");
                return ret;
            }
        };

        // map
        template<typename T>
        struct Writer<T, std::enable_if_t<is_std_map<T>::value, enabler>> {
            static std::string to_string(const T &v) {
                std::string ret;
                ret.append("{");
                for (auto it = std::begin(v); it != std::end(v); it++) {
                    if (it != std::begin(v))
                        ret.append(", ");
                    ret.append(Writer<typename T::value_type, enabler>::to_string(*it));// call pair::to_string
                }
                ret.append("}");
                return ret;
            }
        };

        template<typename T>
        struct Writer<T, std::enable_if_t<!is_number<T>::value && !is_string<T>::value && is_ostreamable<T>::value, enabler>> {
            static std::string to_string(const T &v) {
                std::stringstream ss;
                ss << v;
                return ss.str();
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
                    auto strArg = Writer<T, enabler>::to_string(arg);
                    os.write(strArg.data(), strArg.size());
                    format_impl(os, pos + 2, size - 2 - preBytes);
                } else if (*(pos + 1) == '{') {
                    if ((pos - format + 2 < size) && (*(pos + 2) == '}')) {
                        if ((pos - format + 3 < size) && (*(pos + 3) == '}')) {// {{}}
                            os << "{}";
                            format_impl(os, pos + 4, size - 4 - preBytes, arg);
                        } else {// {{} at end or {{}X
                            os.put('{');
                            auto strArg = Writer<T, enabler>::to_string(arg);
                            os.write(strArg.data(), strArg.size());
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
                    auto strArg = Writer<T, enabler>::to_string(arg);
                    os.write(strArg.data(), strArg.size());
                    format_impl(os, pos + 2, size - 2 - preBytes, std::forward<Args>(args)...);
                } else if (*(pos + 1) == '{') {
                    if ((pos - format + 2 < size) && (*(pos + 2) == '}')) {
                        if ((pos - format + 3 < size) && (*(pos + 3) == '}')) {// {{}}
                            os << "{}";
                            format_impl(os, pos + 4, size - 4 - preBytes, arg, std::forward<Args>(args)...);
                        } else {// {{} at end or {{}X
                            os.put('{');
                            auto strArg = Writer<T, enabler>::to_string(arg);
                            os.write(strArg.data(), strArg.size());
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

    inline std::string sprintf_format(const char *format, ...) {
        char buffer[4096]{0};
        va_list args;
        va_start(args, format);
        vsprintf(buffer, format, args);
        va_end(args);
        return {buffer};
    }

    // todo: thread local buffer for simple_format, reset ostream buffer with thread local buffer
    // todo: float/double precision, eg: format: "{.2}"
}// namespace libs

#endif// LIBS_STRUTILS_H
#pragma once

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Utils {

  inline void format_impl(std::ostream& os, std::string_view format) {
    os << format;
  }

  template <typename T>
  void format_impl(std::ostream& os, std::string_view format, const T& arg) {
    if(auto pos = format.find("{}"); pos != std::string::npos) {
      os << format.substr(0, pos);
      os << arg;
      os << format.substr(pos + 2);
    }
    else {
      os << format;
    }
  }

  template <typename T, typename... Args>
  void format_impl(std::ostream& os, std::string_view format, const T& arg, const Args&... args) {
    // how to print {}?
    // index {}?
    // wide string?
    if(auto pos = format.find("{}"); pos != std::string::npos) {
      os << format.substr(0, pos);
      os << arg;
      format_impl(os, format.substr(pos + 2), args...);
    }
    else {
      // no other placeholder found, stop process more args
      os << format;
      // os << arg;
      // format_impl(os, "", args...);
    }
  }

  template <typename... Args>
  std::string simple_format(const std::string& format, Args... args) {
    std::stringstream ss;
    format_impl(ss, format, args...);
    return ss.str();
  }

  inline std::string simple_format(const std::string& format) {
    std::stringstream ss;
    format_impl(ss, format);
    return ss.str();
  }

  template <typename... Args>
  void easy_print(const std::string& format, Args... args) {
    format_impl(std::cout, format, args...);
    std::cout << std::endl;
  }

  inline void easy_print(const std::string& format) {
    format_impl(std::cout, format);
    std::cout << std::endl;
  }

  inline std::vector<std::string> split_str(const std::string& text, const std::string& delimeters) {
    std::size_t start = 0, end, delimLen = delimeters.length();
    std::string token;
    std::vector<std::string> results;

    while((end = text.find(delimeters, start)) != std::string::npos) {
      token = text.substr(start, end - start);
      start = end + delimLen;
      results.push_back(token);
    }

    results.push_back(text.substr(start));
    return results;
  }

  inline std::string trim_str(const std::string& str) {
    auto trim = [](char c) { return !(std::iscntrl(c) || c == ' '); };
    auto start = std::find_if(str.cbegin(), str.cend(), trim);
    auto end = std::find_if(str.rbegin(), str.rend(), trim);
    if((start == str.cend()) || (end == str.rend()))
      return "";
    return std::string(start, end.base());
  }

  inline std::size_t content_length(std::istream& iss) {
    if(iss.bad())
      return 0;
    iss.seekg(0, std::ios_base::end);
    auto size = iss.tellg();
    iss.seekg(0, std::ios_base::beg);
    return size;
  }

  inline std::size_t content_length(std::ostream& oss) {
    if(oss.bad())
      return 0;
    oss.seekp(0, std::ios_base::end);
    auto size = oss.tellp();
    oss.seekp(0, std::ios_base::beg);
    return size;
  }

  inline std::string extract_stream(std::istream& input) {
    if(input.fail())
      return "";
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
  }

  inline bool str_iequals(const std::string& s1, const std::string& s2) {
  return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(), [](char a, char b) {
    return tolower(a) == tolower(b);
  });
}
} // namespace Utils
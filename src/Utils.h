#pragma once

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace Utils {
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
} // namespace Utils
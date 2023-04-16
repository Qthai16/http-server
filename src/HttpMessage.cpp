#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "HttpMessage.h"

// using namespace HttpMessage;

std::initializer_list<std::string> requiredHeaders = {"Content-Type", "Content-Length", "Connection"};

std::string HttpMessage::method_str(const HttpMessage::HTTPMethod& method) {
  switch(method) {
  case HTTPMethod::GET:
    return "GET";
  case HTTPMethod::HEAD:
    return "HEAD";
  case HTTPMethod::POST:
    return "POST";
  case HTTPMethod::PUT:
    return "PUT";
  case HTTPMethod::DELETE:
    return "DELETE";
  case HTTPMethod::CONNECT:
    return "CONNECT";
  case HTTPMethod::OPTIONS:
    return "OPTIONS";
  case HTTPMethod::TRACE:
    return "TRACE";
  case HTTPMethod::PATCH:
    return "PATCH";
  default:
    return "";
  }
}

std::string HttpMessage::version_str(const HttpMessage::HTTPVersion& version) {
  switch(version) {
  case HTTPVersion::HTTP_1_1:
    return "HTTP/1.1";
  case HTTPVersion::HTTP_2_0:
    return "HTTP/2.0";
  default:
    return "";
  }
}

std::size_t HttpMessage::content_length(std::istream& iss) {
  if(iss.bad())
    return 0;
  iss.seekg(0, std::ios_base::end);
  auto size = iss.tellg();
  iss.seekg(0, std::ios_base::beg);
  return size;
}

HttpMessage::HTTPVersion HttpMessage::str_to_http_version(const std::string& str) {
  static std::map<std::string, HttpMessage::HTTPVersion> convertMap = {
      {"HTTP/1.1", HttpMessage::HTTPVersion::HTTP_1_1},
      {"HTTP/2.0", HttpMessage::HTTPVersion::HTTP_2_0}};
  std::string copyStr;
  std::transform(str.cbegin(), str.cend(), std::back_inserter(copyStr), [](char c) {
    return std::toupper(c);
  });
  if(convertMap.count(copyStr))
    return convertMap.at(copyStr);
  throw std::logic_error("HTTP version not support");
}

HttpMessage::HTTPMethod HttpMessage::str_to_method(const std::string& str) {
  static std::map<std::string, HttpMessage::HTTPMethod> convertMap = {
    {"GET", HttpMessage::HTTPMethod::GET},
    {"HEAD", HttpMessage::HTTPMethod::HEAD},
    {"POST", HttpMessage::HTTPMethod::POST},
    {"PUT", HttpMessage::HTTPMethod::PUT},
    {"DELETE", HttpMessage::HTTPMethod::DELETE},
    {"CONNECT", HttpMessage::HTTPMethod::CONNECT},
    {"OPTIONS", HttpMessage::HTTPMethod::OPTIONS},
    {"TRACE", HttpMessage::HTTPMethod::TRACE},
    {"PATCH", HttpMessage::HTTPMethod::PATCH},
  };
  std::string copyStr;
  std::transform(str.cbegin(), str.cend(), std::back_inserter(copyStr), [](char c) {
    return std::toupper(c);
  });
  if(convertMap.count(copyStr))
    return convertMap.at(copyStr);
  throw std::logic_error("method not support");
}

std::vector<std::string> HttpMessage::split_str(const std::string& text, const std::string& delimeters) {
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

std::string HttpMessage::trim_str(const std::string& str) {
  auto trim = [](char c){return !(std::iscntrl(c) || c == ' ');};
  auto start = std::find_if(str.cbegin(), str.cend(), trim);
  auto end = std::find_if(str.rbegin(), str.rend(), trim);
  if ((start == str.cend()) || (end == str.rend()))
    return "";
  return std::string(start, end.base());
}

// struct HTTPResponse {
//   HTTPVersion _version;
//   HTTPMethod _method;
//   HTTPStatusCode _statusCode;
//   HeadersMap _headers;
//   std::istream& _content;

//   HTTPResponse(std::istream& content) : _version(HTTPVersion::HTTP_1_1),
//                                         _method(HTTPMethod::GET),
//                                         _statusCode(HTTPStatusCode::OK),
//                                         _headers(),
//                                         _content(content) {}

// };
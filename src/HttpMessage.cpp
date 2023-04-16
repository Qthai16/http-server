#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

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
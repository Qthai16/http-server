#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "HttpMessage.h"

// using namespace HttpMessage;

// std::initializer_list<std::string> requiredHeaders = {"Content-Type", "Content-Length", "Connection"};

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
  case HTTPVersion::HTTP_1_0:
    return "HTTP/1.0";
  case HTTPVersion::HTTP_1_1:
    return "HTTP/1.1";
  case HTTPVersion::HTTP_2_0:
    return "HTTP/2.0";
  default:
    return "";
  }
}

std::string HttpMessage::status_code_str(const HttpMessage::HTTPStatusCode& code) {
  switch(code) {
  case HttpMessage::HTTPStatusCode::Continue:
    return "Continue";
  case HttpMessage::HTTPStatusCode::OK:
    return "OK";
  case HttpMessage::HTTPStatusCode::Created:
    return "Created";
  case HttpMessage::HTTPStatusCode::Accepted:
    return "Accepted";
  case HttpMessage::HTTPStatusCode::NonAuthoritativeInformation:
    return "NonAuthoritativeInformation";
  case HttpMessage::HTTPStatusCode::NoContent:
    return "NoContent";
  case HttpMessage::HTTPStatusCode::ResetContent:
    return "ResetContent";
  case HttpMessage::HTTPStatusCode::PartialContent:
    return "PartialContent";
  case HttpMessage::HTTPStatusCode::MultipleChoices:
    return "MultipleChoices";
  case HttpMessage::HTTPStatusCode::MovedPermanently:
    return "MovedPermanently";
  case HttpMessage::HTTPStatusCode::Found:
    return "Found";
  case HttpMessage::HTTPStatusCode::NotModified:
    return "NotModified";
  case HttpMessage::HTTPStatusCode::BadRequest:
    return "BadRequest";
  case HttpMessage::HTTPStatusCode::Unauthorized:
    return "Unauthorized";
  case HttpMessage::HTTPStatusCode::Forbidden:
    return "Forbidden";
  case HttpMessage::HTTPStatusCode::NotFound:
    return "NotFound";
  case HttpMessage::HTTPStatusCode::MethodNotAllowed:
    return "MethodNotAllowed";
  case HttpMessage::HTTPStatusCode::RequestTimeout:
    return "RequestTimeout";
  case HttpMessage::HTTPStatusCode::ImATeapot:
    return "ImATeapot";
  case HttpMessage::HTTPStatusCode::InternalServerError:
    return "InternalServerError";
  case HttpMessage::HTTPStatusCode::NotImplemented:
    return "NotImplemented";
  case HttpMessage::HTTPStatusCode::BadGateway:
    return "BadGateway";
  case HttpMessage::HTTPStatusCode::ServiceUnvailable:
    return "ServiceUnvailable";
  case HttpMessage::HTTPStatusCode::GatewayTimeout:
    return "GatewayTimeout";
  case HttpMessage::HTTPStatusCode::HttpVersionNotSupported:
    return "HttpVersionNotSupported";
  default:
    return "";
  }
}

HttpMessage::HTTPVersion HttpMessage::str_to_http_version(const std::string& str) {
  static std::map<std::string, HttpMessage::HTTPVersion> convertMap = {
      {"HTTP/1.0", HttpMessage::HTTPVersion::HTTP_1_0},
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

std::string HttpMessage::headers_get_field(const HttpMessage::HeadersMap& headers, std::string key) {
  auto toLowerCase = [](std::string str) -> std::string {
    std::string outStr;
    std::transform(str.cbegin(), str.cend(), std::back_inserter(outStr), [](char c) {
      return std::tolower(c);
    });
    return outStr;
  };
  auto iter = std::find_if(headers.cbegin(), headers.cend(), [key, toLowerCase](auto pair){
    return toLowerCase(key) == toLowerCase(pair.first);
  });
  if (iter != headers.cend())
    return iter->second;
  return "";
}

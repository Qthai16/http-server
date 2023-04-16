#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>


namespace HttpMessage {
  using HandlerFunction = std::function<void(int)>;
  using HeadersMap = std::map<std::string, std::string>;

  enum class HTTPMethod {
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    CONNECT,
    OPTIONS,
    TRACE,
    PATCH
  };

  enum class HTTPVersion {
    HTTP_1_1 = 11,
    HTTP_2_0 = 20
  };

  enum class HTTPStatusCode {
    Continue = 100,
    OK = 200,
    Created = 201,
    Accepted = 202,
    NonAuthoritativeInformation = 203,
    NoContent = 204,
    ResetContent = 205,
    PartialContent = 206,
    MultipleChoices = 300,
    MovedPermanently = 301,
    Found = 302,
    NotModified = 304,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    RequestTimeout = 408,
    ImATeapot = 418,
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnvailable = 503,
    GatewayTimeout = 504,
    HttpVersionNotSupported = 505
  };

  std::string method_str(const HTTPMethod& method);
  std::string version_str(const HTTPVersion& version);
  std::size_t content_length(std::istream& iss);
  HTTPVersion str_to_http_version(const std::string& str);
  HTTPMethod str_to_method(const std::string& str);
  std::vector<std::string> split_str(const std::string& text, const std::string& delimeters);
  std::string trim_str(const std::string& str);

  struct HTTPResponse {
    HTTPVersion _version;
    HTTPStatusCode _statusCode;
    HeadersMap _headers;
    std::istream& _content;

    HTTPResponse(std::istream& content) : _version(HTTPVersion::HTTP_1_1),
                                          _statusCode(HTTPStatusCode::OK),
                                          _headers(),
                                          _content(content) {}

    friend std::ostream& operator<<(std::ostream& os, const HTTPResponse& response) {
      // serialize to stream
      os << "HTTP/1.1 200 OK\r\n"; // for testing only
      for(const auto& [key, value] : response._headers) {
        os << key << ": " << value << "\r\n";
      }
      os << "\r\n";
      os << response._content.rdbuf();
      return os;
    }
  };

  struct HTTPRequest {
    HTTPVersion _version;
    HTTPMethod _method;
    HeadersMap _headers;
    std::string _uri;
    std::istream& _request;

    HTTPRequest(std::istream& request) : _version(HTTPVersion::HTTP_1_1),
                                         _method(HTTPMethod::GET),
                                         _headers(),
                                         _uri(),
                                         _request(request) {
      _request >> *this;
    }

    friend std::istream& operator>>(std::istream& is, HTTPRequest& obj) {
      // read obj from stream
      if(is.bad())
        return is;

      std::string line;
      std::string firstLine;
      // first line is [method URI version]
      if(std::getline(is, firstLine, '\n')) {
        auto tokens = split_str(firstLine, " ");
        if(tokens.size() != 3) {
          std::cerr << "Error, invalid http message\n";
          throw std::runtime_error("invalid http message");
        }
        obj._method = str_to_method(tokens[0]);
        obj._uri = tokens[1];
        obj._version = str_to_http_version(tokens[2]);
      }
      // next following lines are headers
      while(std::getline(is, line, '\n')) {
        auto trimLine = trim_str(line);
        if(trimLine.empty())
          break;
        auto delimPos = trimLine.find(':');
        auto header = trimLine.substr(0, delimPos);
        auto value = trim_str(trimLine.substr(delimPos + 1));
        obj._headers[header] = value;
      }

      // remaining line is body
      return is;
    }
  };

} // namespace HttpMessage
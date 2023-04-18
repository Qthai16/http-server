#pragma once

#include <arpa/inet.h>
#include <functional>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "Utils.h"

namespace HttpMessage {
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
    HTTP_1_0 = 10,
    HTTP_1_1 = 11,
    HTTP_2_0 = 20
  };

  enum HTTPStatusCode {
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
  std::string status_code_str(const HTTPStatusCode& code);
  HTTPVersion str_to_http_version(const std::string& str);
  HTTPMethod str_to_method(const std::string& str);

  struct HTTPResponse {
    HTTPVersion _version;
    HTTPStatusCode _statusCode;
    HeadersMap _headers;
    std::string _body;

    HTTPResponse(int clientFd) :
        _version(HTTPVersion::HTTP_1_1),
        _statusCode(HTTPStatusCode::OK),
        _headers(),
        _body() {}

    friend std::ostream& operator<<(std::ostream& responseStream, HTTPResponse& response) {
      // serialize to stream
      responseStream << version_str(response._version) << " " << std::to_string(response._statusCode) << " " << status_code_str(response._statusCode) << "\r\n";
      if(response._body.empty())
        response._headers["Content-Length"] = "0";
      else
        response._headers["Content-Length"] = std::to_string(response._body.length());
      for(const auto& [key, value] : response._headers) {
        responseStream << key << ": " << value << "\r\n";
      }
      responseStream << "\r\n";
      responseStream << response._body;
      responseStream << "\r\n";
      responseStream.flush();
      return responseStream;
    }

    HTTPResponse& status_code(HTTPStatusCode statusCode) {
      _statusCode = statusCode;
      return *this;
    }

    HTTPResponse& body(const std::string& content) {
      _body = std::move(content);
      return *this;
    }
  };

  struct HTTPRequest {
    HTTPVersion _version;
    HTTPMethod _method;
    HeadersMap _headers;
    HeadersMap _queryParams;
    std::string _path;
    std::stringstream _body; // may have RAM issues with large request body
    std::istream& _request;

    HTTPRequest(std::istream& request) :
        _version(HTTPVersion::HTTP_1_1),
        _method(HTTPMethod::GET),
        _headers(),
        _queryParams(),
        _path(),
        _body(),
        _request(request) {
      // _request >> *this;
      parse_request(_request);
    }

    void parse_query_params(const std::string& path) {
      auto startPos = path.find("?");
      if(startPos == std::string::npos)
        return;
      auto queries = path.substr(startPos + 1);
      auto pairs = Utils::split_str(queries, "&");
      for(const auto& pair : pairs) {
        auto delimPos = pair.find("=");
        if(delimPos != std::string::npos)
          _queryParams[pair.substr(0, delimPos)] = pair.substr(delimPos + 1);
      }
    }

    // friend std::istream& operator>>(std::istream& is, HTTPRequest& obj) {
    void parse_request(std::istream& is) {
      if(is.bad())
        return;

      std::string line;
      std::string firstLine;
      // first line is [method path version]
      if(std::getline(is, firstLine, '\n')) {
        auto tokens = Utils::split_str(firstLine, " ");
        if(tokens.size() != 3) {
          std::cerr << "Error, invalid http message\n";
          throw std::runtime_error("invalid http message");
        }
        _method = str_to_method(tokens[0]);
        _path = tokens[1];
        parse_query_params(_path);
        _version = str_to_http_version(Utils::trim_str(tokens[2]));
      }
      // next following lines are headers
      while(std::getline(is, line, '\n')) {
        auto trimLine = Utils::trim_str(line);
        if(trimLine.empty())
          break;
        auto delimPos = trimLine.find(':');
        auto header = trimLine.substr(0, delimPos);
        auto value = Utils::trim_str(trimLine.substr(delimPos + 1));
        _headers[header] = value;
      }

      // remaining lines is body
      while(std::getline(is, line)) {
        _body << line;
      }
      // return is;
    }

    std::string to_string() { // for debug only
      std::cout << "version: " << version_str(_version) << ", method: " << method_str(_method) << "\n";
      std::cout << "path: " << _path << "\n";
      std::cout << "HEADERS: " << std::endl;
      for(const auto& [key, value] : _headers) {
        std::cout << key << ": " << value << '\n';
      }
      std::cout << "QUERY PARAMS: " << std::endl;
      for(const auto& [key, value] : _queryParams) {
        std::cout << key << ": " << value << '\n';
      }
      std::string body = _body.str();
      std::cout << "BODY: " << std::endl;
      std::cout << body << std::endl;
      return "";
    }
  };

} // namespace HttpMessage
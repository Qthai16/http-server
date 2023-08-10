#pragma once

#include <arpa/inet.h>
#include <fstream>
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
  std::string headers_get_field(const HeadersMap& headers, std::string key);

  struct HTTPResponse {
    HTTPVersion _version;
    HTTPStatusCode _statusCode;
    HeadersMap _headers;
    std::ifstream _body;
    bool _finishWriteHeader;
    std::streampos _totalWrite;
    std::size_t _contentLength;

    HTTPResponse(int clientFd) :
        _version(HTTPVersion::HTTP_1_1),
        _statusCode(HTTPStatusCode::OK),
        _headers(),
        _body(),
        _finishWriteHeader(false),
        _totalWrite(0),
        _contentLength(0) {}

    ~HTTPResponse() {
      if(_body.is_open())
        _body.close();
    }

    // HTTPResponse(const HTTPResponse&) {}
    // HTTPResponse(HTTPResponse&&) {}

    const HTTPResponse& operator=(HTTPResponse response) { return *this; }

    // friend std::ostream& operator<<(std::ostream& responseStream, HTTPResponse& response) {
    std::string serialize_header(char* buffer, std::size_t size) {
      std::stringstream ss;
      ss << version_str(_version) << " " << std::to_string(_statusCode) << " " << status_code_str(_statusCode) << "\r\n";
      _headers["Content-Length"] = std::to_string(_contentLength);
      for(const auto& [key, value] : _headers) {
        ss << key << ": " << value << "\r\n";
      }
      ss << "\r\n";
      // ss.seekg(0, std::ios_base::end);
      // auto headerSize = ss.tellg();
      // ss.seekg(0, std::ios_base::beg);
      // ss.read(buffer, headerSize);
      auto text = ss.str();
      text.copy(buffer, text.size());
      _finishWriteHeader = true;
      return text;
    }

    std::size_t serialize_reponse(char* buffer, std::size_t bufferSize) {
      std::string headers;
      if(!_finishWriteHeader)
        headers = serialize_header(buffer, bufferSize);
      auto headerSize = headers.length();
      auto remainBufferBytes = bufferSize - headerSize;
      if(remainBufferBytes <= 0) {
        throw std::logic_error("buffer to small");
        return 0;
      }
      if(remainBufferBytes > _contentLength) {
        // buffer is enough to write whole response
        _body.read(buffer + headerSize, _contentLength);
        _totalWrite = _contentLength;
        return _contentLength + headerSize;
      }
      else {
        if(remainBufferBytes < bufferSize) {
          // body first chunk
          _body.read(buffer + headerSize, bufferSize - headerSize);
          _totalWrite += bufferSize - headerSize;
          return bufferSize;
        }
        else {
          if(_contentLength - _body.tellg() > bufferSize) {
            _body.read(buffer, bufferSize);
            _totalWrite += bufferSize;
            return bufferSize;
          }
          else { // last chunk
            _body.read(buffer, _contentLength - _body.tellg());
            _totalWrite = _contentLength;
            return _contentLength - _body.tellg();
          }
        }
      }
    }

    void status_code(HTTPStatusCode statusCode) {
      _statusCode = statusCode;
    }

    void set_str_body(std::string content) {
      _body = std::ifstream(content);
      _contentLength = Utils::content_length(_body);
    }

    void set_file_body(std::string path) {
      _body.open(path);
      _contentLength = Utils::content_length(_body);
    }
  };

  struct HTTPRequest {
    HTTPVersion _version;
    HTTPMethod _method;
    HeadersMap _headers;
    HeadersMap _queryParams;
    std::string _path;
    std::stringstream _body;
    std::stringstream _bufferStream;
    std::size_t _totalRead;
    bool _finishParseHeaders;

    HTTPRequest() :
        _version(HTTPVersion::HTTP_1_1),
        _method(HTTPMethod::GET),
        _headers(),
        _queryParams(),
        _path(),
        _body(),
        _bufferStream(),
        _totalRead(0),
        _finishParseHeaders(false) {}

    ~HTTPRequest() = default;

    std::size_t content_length() {
      auto lenStr = headers_get_field(_headers, "Content-Length");
      if(lenStr.empty())
        return 0;
      return stoul(lenStr);
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

    void parse_headers(std::istream& is) {
      if(is.bad())
        return;
      std::string line;
      std::string firstLine;
      // first line is [method path version]
      if(std::getline(is, firstLine, '\n')) {
        auto tokens = Utils::split_str(firstLine, " ");
        if(tokens.size() != 3) {
          std::cerr << "Error, invalid http message" << std::endl;
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
      _finishParseHeaders = true;
    }

    void parse_request(std::istream& is) {
      if(!_finishParseHeaders)
        parse_headers(is);
      // remaining lines is body
      _body << is.rdbuf();
      _totalRead += Utils::content_length(is);
    }

    void parse_request() {
      parse_request(_bufferStream);
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
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
#include <cstring>

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
    enum class ReadType {
      UNINIT = -1,
      FILE_READ = 0,
      IN_MEMORY_READ = 1
    };
    HTTPVersion _version;
    HTTPStatusCode _statusCode;
    HeadersMap _headers;
    std::ifstream _body; // bad, should refactor later
    std::stringstream _inMemoryBody;
    ReadType _readType;
    bool _finishWriteHeader;
    std::size_t _totalWrite;
    std::size_t _contentLength;

    HTTPResponse() :
        _version(HTTPVersion::HTTP_1_1),
        _statusCode(HTTPStatusCode::OK),
        _headers(),
        _body(),
        _inMemoryBody(),
        _readType(ReadType::UNINIT),
        _finishWriteHeader(false),
        _totalWrite(0),
        _contentLength(0) {}

    ~HTTPResponse() {
      if(_body.is_open())
        _body.close();
    }

    HTTPResponse(const HTTPResponse&) = delete;
    const HTTPResponse& operator=(const HTTPResponse& other) = delete;
    HTTPResponse(HTTPResponse&& other) {
      _version = other._version;
      _statusCode = other._statusCode;
      _headers = other._headers;
      _body = std::move(other._body);
      _inMemoryBody = std::move(other._inMemoryBody);
      _readType = other._readType;
      _finishWriteHeader = other._finishWriteHeader;
      _totalWrite = other._totalWrite;
      _contentLength = other._contentLength;
    }

    // friend std::ostream& operator<<(std::ostream& responseStream, HTTPResponse& response) {
    std::size_t serialize_header(char* buffer, std::size_t bufferSize) {
      std::stringstream ss;
      Utils::format_impl(ss, "{} {} {}\r\n", version_str(_version), std::to_string(_statusCode), status_code_str(_statusCode));
      // ss << version_str(_version) << " " << std::to_string(_statusCode) << " " << status_code_str(_statusCode) << "\r\n";
      _headers["Content-Length"] = std::to_string(_contentLength);
      for(const auto& [key, value] : _headers) {
        // ss << key << ": " << value << "\r\n";
        Utils::format_impl(ss, "{}: {}\r\n", key, value);
      }
      ss << "\r\n";
      // ss.seekg(0, std::ios_base::end);
      // auto headerSize = ss.tellg();
      // ss.seekg(0, std::ios_base::beg);
      // ss.read(buffer, headerSize);
      auto text = ss.str();
      auto headerSize = text.size();
      if(headerSize > bufferSize) {
        throw std::logic_error("buffer to small");
        return headerSize;
      }
      text.copy(buffer, headerSize);
      _finishWriteHeader = true;
      return headerSize;
    }

    std::size_t serialize_body(std::istream& is, char* buffer, std::size_t bufferSize, std::size_t headerSize) {
      auto remainBytes = bufferSize - headerSize;
      if(remainBytes <= 0) {
        throw std::logic_error("buffer to small");
        return 0;
      }
      if(remainBytes > _contentLength) {
        // buffer is enough to write whole response
        is.read(buffer + headerSize, _contentLength);
        _totalWrite = _contentLength;
        return _contentLength + headerSize;
      }
      else {
        if(remainBytes < bufferSize) { // first chunk
          is.read(buffer + headerSize, bufferSize - headerSize);
          _totalWrite += bufferSize - headerSize;
          return bufferSize;
        }
        else {
          if(_contentLength - is.tellg() > bufferSize) {
            is.read(buffer, bufferSize);
            _totalWrite += bufferSize;
            return bufferSize;
          }
          else { // last chunk
            auto lastBytes = _contentLength - is.tellg();
            is.read(buffer, lastBytes);
            _totalWrite = _contentLength;
            return lastBytes;
          }
        }
      }
    }

    std::size_t serialize_reponse(char* buffer, std::size_t bufferSize) {
      std::size_t headerSize = 0;
      std::size_t writeCount = 0;
      if(!_finishWriteHeader)
        headerSize = serialize_header(buffer, bufferSize);

      if(_readType == ReadType::FILE_READ) {
        writeCount = serialize_body(_body, buffer, bufferSize, headerSize);
      }
      else if(_readType == ReadType::IN_MEMORY_READ) {
        writeCount = serialize_body(_inMemoryBody, buffer, bufferSize, headerSize);
      }
      else { // empty body?
      }
      return writeCount;
    }

    void status_code(HTTPStatusCode statusCode) {
      _statusCode = statusCode;
    }

    void set_str_body(const std::string& content) {
      _readType = ReadType::IN_MEMORY_READ;
      _inMemoryBody << content;
      _contentLength = content.length();
    }

    void set_file_body(std::string path) {
      _readType = ReadType::FILE_READ;
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
    std::stringstream _bufferStream;
    std::size_t _totalRead;
    bool _expectContinue;
    bool _finishParseHeaders;

    HTTPRequest() :
        _version(HTTPVersion::HTTP_1_1),
        _method(HTTPMethod::GET),
        _headers(),
        _queryParams(),
        _path(),
        _bufferStream(),
        _totalRead(0),
        _expectContinue(false),
        _finishParseHeaders(false) {}

    ~HTTPRequest() = default;

    std::size_t content_length() const {
      auto lenStr = headers_get_field(_headers, "Content-Length");
      if(lenStr.empty())
        return 0;
      return stoul(lenStr);
    }

    std::string content_filename() const {
      // Content-Disposition: attachment; filename=FILENAME
      auto contentDisposition = headers_get_field(_headers, "Content-Disposition");
      if(contentDisposition.empty())
        return "";
      auto ind = contentDisposition.find("filename=");
      if (ind != std::string::npos)
        return contentDisposition.substr(ind+strlen("filename="));
      else return contentDisposition;
    }

    bool expect_100_continue() const {
      auto expectStatus = headers_get_field(_headers, "Expect");
      if(expectStatus.empty())
        return false;
      return Utils::str_iequals("100-continue", expectStatus);
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

    std::streampos parse_headers(std::istream& is) {
      std::string line;
      std::string firstLine;
      // first line is [method path version]
      if(std::getline(is, firstLine, '\n')) {
        auto tokens = Utils::split_str(firstLine, " ");
        if(tokens.size() != 3) {
          std::cerr << "Error, invalid http message" << std::endl;
          throw std::runtime_error("invalid http message");
          return 0;
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
      _expectContinue = expect_100_continue();
      _finishParseHeaders = true;
      return is.tellg();
    }

    std::size_t parse_request(std::ostream& os, char* buffer, std::size_t bytesCount) {
      std::streampos headerSize = 0;
      if(!_finishParseHeaders) {
        std::stringstream tempBuffer;
        tempBuffer.write(buffer, bytesCount);
        headerSize = parse_headers(tempBuffer);
        // write remaining body bytes to os stream
        if(!expect_100_continue()) {
          auto wholeStr = tempBuffer.str();
          auto bodyPart = wholeStr.substr(headerSize);
          os.write(bodyPart.data(), bodyPart.size());
        }
      }
      else {
        os.write(buffer, bytesCount);
      }
      _totalRead += bytesCount - headerSize;
      return _totalRead;
    }

    // for debug and logging
    std::string to_string(std::ostream& os = std::cout) {
      Utils::format_impl(os, "version: {}, method: {}, path: {}\r\n", version_str(_version), method_str(_method), _path);
      Utils::format_impl(os, "headers: [\r\n");
      for(const auto& [key, value] : _headers) {
        Utils::format_impl(os, "{}: {}\n", key, value);
      }
      Utils::format_impl(os, "]\r\n");

      Utils::format_impl(os, "query params: [\r\n");
      for(const auto& [key, value] : _queryParams) {
        Utils::format_impl(os, "{}: {}\n", key, value);
      }
      Utils::format_impl(os, "]\r\n");
      // std::string body = _body.str();
      // std::cout << "BODY: " << std::endl;
      // std::cout << body << std::endl;
      return ""; // change to void?
    }

    std::string to_json(std::ostream& os = std::cout) {
      // args: version, method, path, headers, query_parm
      auto jsonStrFormat = R"JSON({"version":"{}","method":"{}","path":"{}","headers":[{}],"query_params":[{}]})JSON";
      auto serialize_dict = [](const HeadersMap& dict) -> std::string {
        if(dict.empty())
          return "";
        std::string serializeStr = "{";
        for(const auto& [key, value] : dict) {
          serializeStr += Utils::simple_format(R"JSON("{}": "{}",)JSON", key, value);
        }
        serializeStr.pop_back(); // remove last comma (,)
        serializeStr.push_back('}');
        return serializeStr;
      };
      Utils::format_impl(os, jsonStrFormat,
                         version_str(_version), method_str(_method), _path, serialize_dict(_headers), serialize_dict(_queryParams));
      return ""; // change to void?
    }
  };

} // namespace HttpMessage
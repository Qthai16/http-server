#pragma once

#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <algorithm>
#include <streambuf>
#include <memory>

#include "MemBuffer.h"
namespace libs::http {
    using HeadersMap = std::map<std::string, std::string>;
    using BufferType = libs::MemBuf;

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

    enum HTTPCode : uint16_t {
        CODE_100 = 100,
        CODE_200 = 200,
        CODE_201 = 201,
        CODE_202 = 202,
        CODE_203 = 203,
        CODE_204 = 204,
        CODE_205 = 205,
        CODE_206 = 206,
        CODE_300 = 300,
        CODE_301 = 301,
        CODE_302 = 302,
        CODE_304 = 304,
        CODE_400 = 400,
        CODE_401 = 401,
        CODE_403 = 403,
        CODE_404 = 404,
        CODE_405 = 405,
        CODE_408 = 408,
        CODE_418 = 418,
        CODE_500 = 500,
        CODE_501 = 501,
        CODE_502 = 502,
        CODE_503 = 503,
        CODE_504 = 504,
        CODE_505 = 505
    };

    std::string method_str(const HTTPMethod &method);
    std::string version_str(const HTTPVersion &version);
    std::string status_code_str(const HTTPCode &code);
    std::pair<bool, HTTPVersion> str_to_http_version(const std::string &str);
    std::pair<bool, HTTPMethod> str_to_method(const std::string &str);
    std::string headers_get_field(const HeadersMap &headers, std::string key);

    class HTTPResponse {
    public:
        enum class ReadType {
            UNINIT = -1,
            FILE_READ = 0,
            IN_MEMORY_READ = 1
        };

        HTTPResponse(size_t bufsize);
        ~HTTPResponse();
        HTTPResponse(const HTTPResponse &) = delete;
        const HTTPResponse &operator=(const HTTPResponse &other) = delete;
        HTTPResponse(HTTPResponse &&other);
        HTTPResponse& operator=(HTTPResponse &&other);

    public: // for server and io worker
        void write_reponse();
        std::shared_ptr<BufferType> get_buf() const;
        bool write_done() const;
        void resetData();

    public:// for handler
        void http_code(HTTPCode httpCode);
        void str_body(const std::string &content);
        void str_body(const char* buf, size_t size);
        void file_body(std::string path);
        void insert_header(std::pair<std::string, std::string> val);

    private:
        void init_buffer(size_t size);
        void write_header();
        void write_mem_body();
        void write_file_body();

    private:
        // static size_t inline maxBufferSize_ = (2 << 16) - 1;
        HTTPVersion _version;
        HTTPCode _httpCode;
        HeadersMap _headers;

        std::shared_ptr<BufferType> buffer_;
        std::string memBody_;
        int fileFd_;
        int64_t wrOff_{0};
        ReadType _readType;
        bool _finishWriteHeader;
        std::size_t _totalWrite{0};
        std::size_t _contentLength{0};
    };

    struct HTTPRequest {
        HTTPRequest(size_t bufSize);
        ~HTTPRequest() = default;

        std::string get_header(const std::string &key) const;
        std::size_t content_length() const;
        std::string content_filename() const;
        bool expect_100_continue() const;
        bool request_completed() const;
        bool have_expect_continue() const;
        void resetData();

        void parse_query_params(const std::string &path);
        std::size_t parse_headers(const char *buffer, std::size_t bufsize);
        std::size_t parse_request();
        libs::MemBuf* get_buf();
        std::string body_str() const;
        // for debug and logging
        std::string to_string(std::ostream &os = std::cout);
        std::string to_json(std::ostream &os = std::cout);

        HTTPVersion _version;
        HTTPMethod _method;
        HeadersMap _headers;
        HeadersMap _queryParams;
        std::string _path;
        libs::MemBuf recv_buf_;
        libs::MemBuf body_buf_;
        std::size_t _totalRead;
        std::size_t _headerSize;
        std::size_t _contentLength;
        bool _expectContinue;
        bool _finishParseHeaders;
    };

}// namespace libs::http

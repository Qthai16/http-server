#pragma once

#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <algorithm>
#include <streambuf>

namespace simple_http {
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

    std::string method_str(const HTTPMethod &method);
    std::string version_str(const HTTPVersion &version);
    std::string status_code_str(const HTTPStatusCode &code);
    std::pair<bool, HTTPVersion> str_to_http_version(const std::string &str);
    std::pair<bool, HTTPMethod> str_to_method(const std::string &str);
    std::string headers_get_field(const HeadersMap &headers, std::string key);

    struct BufferType {
        char *buf;
        size_t len;
    };

    class HTTPResponse {
    public:
        enum class ReadType {
            UNINIT = -1,
            FILE_READ = 0,
            IN_MEMORY_READ = 1
        };

        HTTPResponse();
        ~HTTPResponse();
        HTTPResponse(const HTTPResponse &) = delete;
        const HTTPResponse &operator=(const HTTPResponse &other) = delete;
        HTTPResponse(HTTPResponse &&other);

    public:                                                                 // for server and io worker
        std::size_t serialize_reponse(char *buffer, std::size_t bufferSize);// todo: this should return Buffer{const char* data, size_t len} (view only data)
        bool writeDone() const;
        void resetData();

    public:// for handler
        void status_code(HTTPStatusCode statusCode);
        void set_str_body(const std::string &content);
        void set_file_body(std::string path);
        void insert_header(std::pair<std::string, std::string> val);

    private:
        std::size_t serialize_header(char *buffer, std::size_t bufferSize);
        std::size_t serialize_body(std::istream &is, char *buffer, std::size_t bufferSize);

    private:
        HTTPVersion _version;
        HTTPStatusCode _statusCode;
        HeadersMap _headers;
        std::ifstream _body;// bad, should refactor later
        std::stringstream _inMemoryBody;

        std::string memBody_;
        int fileFd_;
        int64_t fileOff_;
        ReadType _readType;
        bool _finishWriteHeader;
        std::size_t _totalWrite;
        std::size_t _contentLength;
    };

    struct HTTPRequest {
        HTTPRequest();
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
        std::size_t parse_request(char *buffer, std::size_t bytesCount);
        // BufferType parse_request(MemBuf *membuf);
        // for debug and logging
        std::string to_string(std::ostream &os = std::cout);
        std::string to_json(std::ostream &os = std::cout);

        HTTPVersion _version;
        HTTPMethod _method;
        HeadersMap _headers;
        HeadersMap _queryParams;
        std::string _path;
        std::stringstream _body;
        std::size_t _totalRead;
        std::size_t _contentLength;
        bool _expectContinue;
        bool _finishParseHeaders;
    };

}// namespace simple_http
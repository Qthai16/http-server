#include "HttpMessage.h"

#include "libs/StrUtils.h"
#include "libs/FileUtils.h"

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace simple_http {

    // std::initializer_list<std::string> requiredHeaders = {"Content-Type", "Content-Length", "Connection"};

    std::string method_str(const HTTPMethod &method) {
        switch (method) {
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

    std::string version_str(const HTTPVersion &version) {
        switch (version) {
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

    std::string status_code_str(const HTTPStatusCode &code) {
        switch (code) {
            case HTTPStatusCode::Continue:
                return "Continue";
            case HTTPStatusCode::OK:
                return "OK";
            case HTTPStatusCode::Created:
                return "Created";
            case HTTPStatusCode::Accepted:
                return "Accepted";
            case HTTPStatusCode::NonAuthoritativeInformation:
                return "NonAuthoritativeInformation";
            case HTTPStatusCode::NoContent:
                return "NoContent";
            case HTTPStatusCode::ResetContent:
                return "ResetContent";
            case HTTPStatusCode::PartialContent:
                return "PartialContent";
            case HTTPStatusCode::MultipleChoices:
                return "MultipleChoices";
            case HTTPStatusCode::MovedPermanently:
                return "MovedPermanently";
            case HTTPStatusCode::Found:
                return "Found";
            case HTTPStatusCode::NotModified:
                return "NotModified";
            case HTTPStatusCode::BadRequest:
                return "BadRequest";
            case HTTPStatusCode::Unauthorized:
                return "Unauthorized";
            case HTTPStatusCode::Forbidden:
                return "Forbidden";
            case HTTPStatusCode::NotFound:
                return "NotFound";
            case HTTPStatusCode::MethodNotAllowed:
                return "MethodNotAllowed";
            case HTTPStatusCode::RequestTimeout:
                return "RequestTimeout";
            case HTTPStatusCode::ImATeapot:
                return "ImATeapot";
            case HTTPStatusCode::InternalServerError:
                return "InternalServerError";
            case HTTPStatusCode::NotImplemented:
                return "NotImplemented";
            case HTTPStatusCode::BadGateway:
                return "BadGateway";
            case HTTPStatusCode::ServiceUnvailable:
                return "ServiceUnvailable";
            case HTTPStatusCode::GatewayTimeout:
                return "GatewayTimeout";
            case HTTPStatusCode::HttpVersionNotSupported:
                return "HttpVersionNotSupported";
            default:
                return "";
        }
    }

    std::ostream &operator<<(std::ostream &os, HTTPMethod method) {
        os << method_str(method);
        return os;
    }
    std::ostream &operator<<(std::ostream &os, HTTPVersion ver) {
        os << version_str(ver);
        return os;
    }
    std::ostream &operator<<(std::ostream &os, HTTPStatusCode code) {
        os << status_code_str(code);
        return os;
    }

    HTTPVersion str_to_http_version(const std::string &str) {
        static std::map<std::string, HTTPVersion> convertMap = {
                {"HTTP/1.0", HTTPVersion::HTTP_1_0},
                {"HTTP/1.1", HTTPVersion::HTTP_1_1},
                {"HTTP/2.0", HTTPVersion::HTTP_2_0}};
        std::string copyStr;
        std::transform(str.cbegin(), str.cend(), std::back_inserter(copyStr), [](char c) {
            return std::toupper(c);
        });
        if (convertMap.count(copyStr))
            return convertMap.at(copyStr);
        throw std::logic_error("HTTP version not support");
    }

    HTTPMethod str_to_method(const std::string &str) {
        static std::map<std::string, HTTPMethod> convertMap = {
                {"GET", HTTPMethod::GET},
                {"HEAD", HTTPMethod::HEAD},
                {"POST", HTTPMethod::POST},
                {"PUT", HTTPMethod::PUT},
                {"DELETE", HTTPMethod::DELETE},
                {"CONNECT", HTTPMethod::CONNECT},
                {"OPTIONS", HTTPMethod::OPTIONS},
                {"TRACE", HTTPMethod::TRACE},
                {"PATCH", HTTPMethod::PATCH},
        };
        std::string copyStr;
        std::transform(str.cbegin(), str.cend(), std::back_inserter(copyStr), [](char c) {
            return std::toupper(c);
        });
        if (convertMap.count(copyStr))
            return convertMap.at(copyStr);
        throw std::logic_error("method not support");
    }

    std::string headers_get_field(const HeadersMap &headers, std::string key) {
        auto iter = std::find_if(headers.cbegin(), headers.cend(), [key](auto pair) {
            return libs::str_iequals(key, pair.first);
        });
        if (iter != headers.cend())
            return iter->second;
        return "";
    }

    HTTPRequest::HTTPRequest() : _version(HTTPVersion::HTTP_1_1),
                                 _method(HTTPMethod::GET),
                                 _headers(),
                                 _queryParams(),
                                 _path(),
                                 _body(),
                                 _totalRead(0),
                                 _contentLength(0),
                                 _expectContinue(false),
                                 _finishParseHeaders(false) {}

    std::string HTTPRequest::get_header(const std::string &key) const {
        return headers_get_field(_headers, key);
    }

    std::size_t HTTPRequest::content_length() const {
        auto lenStr = headers_get_field(_headers, "Content-Length");
        if (lenStr.empty())
            return 0;
        return stoul(lenStr);
    }

    std::string HTTPRequest::content_filename() const {
        // Content-Disposition: attachment; name="file_name"; filename="file_name"
        // header filename?
        auto contentDisposition = headers_get_field(_headers, "Content-Disposition");
        if (contentDisposition.empty())
            return "";
        auto ind = contentDisposition.find("filename=");
        if (ind != std::string::npos) {
            auto filename = contentDisposition.substr(ind + strlen("filename="));
            if (filename[0] == '"')
                filename.erase(filename.begin());// confuse erase overloaded
            if (filename[filename.size() - 1] == '"')
                filename.pop_back();
            return filename;
        } else
            return contentDisposition;
    }

    bool HTTPRequest::expect_100_continue() const {
        auto expectStatus = headers_get_field(_headers, "Expect");
        if (expectStatus.empty())
            return false;
        return libs::str_iequals("100-continue", expectStatus);
    }

    bool HTTPRequest::request_completed() const {
        return _finishParseHeaders && (_totalRead >= _contentLength);
    }

    bool HTTPRequest::have_expect_continue() const {
        return _finishParseHeaders && _expectContinue;
    }

    void HTTPRequest::resetData() {
        _headers.clear();
        _queryParams.clear();
        _path.clear();
        _body.seekg(0, std::ios_base::beg);
        _body.seekp(0, std::ios_base::beg);
        _body.clear();
        _totalRead = 0;
        _contentLength = 0;
        _expectContinue = false;
        _finishParseHeaders = false;
    }

    void HTTPRequest::parse_query_params(const std::string &path) {
        auto startPos = path.find("?");
        if (startPos == std::string::npos)
            return;
        auto queries = path.substr(startPos + 1);
        auto pairs = libs::split_str(queries, "&");
        for (const auto &pair: pairs) {
            auto delimPos = pair.find("=");
            if (delimPos != std::string::npos)
                _queryParams[pair.substr(0, delimPos)] = pair.substr(delimPos + 1);
        }
    }

    std::size_t HTTPRequest::parse_headers(const char *buffer, std::size_t bufsize) {
        std::string line;
        std::size_t totalCnt = 0;
        std::size_t cnt = 0;
        auto getLine = [](const char *buf, std::size_t size, std::string &out) -> std::size_t {
            auto i = 0;
            auto ret = 0;
            do {
                if (buf[i] == '\n') {
                    ret += 1;
                    return ret;
                }
                out.push_back(buf[i]);
                ret += 1;
            } while (++i < size);
            return ret;
        };
        // first line is [method path version]
        cnt = getLine(buffer, bufsize, line);
        if (line.empty()) return 0;
        else {
            auto tokens = libs::split_str(line, " ");
            if (tokens.size() != 3)
                throw std::runtime_error("invalid http message");
            _method = str_to_method(tokens[0]);
            _path = tokens[1];
            parse_query_params(_path);
            _version = str_to_http_version(libs::trim(tokens[2]));
        }
        totalCnt += cnt;
        buffer += cnt;
        bufsize -= cnt;
        // remain are headers
        do {
            line.clear();
            cnt = getLine(buffer, bufsize, line);
            if (!line.empty()) {
                totalCnt += cnt;
                buffer += cnt;
                bufsize -= cnt;
                // parse header
                auto trimLine = libs::trim(line);
                if (trimLine.empty())
                    break;
                auto delimPos = trimLine.find(':');
                auto header = trimLine.substr(0, delimPos);
                auto tmp = trimLine.substr(delimPos + 1);
                auto value = libs::trim(tmp);
                _headers[header] = value;
            }
        } while (bufsize > 0);
        _expectContinue = expect_100_continue();
        _contentLength = content_length();
        _finishParseHeaders = true;
        return totalCnt;
    }

    std::size_t HTTPRequest::parse_request(char *buffer, std::size_t bytesCount) {
        std::streampos headerSize = 0;
        auto oldVal = _finishParseHeaders;
        if (!_finishParseHeaders) {
            headerSize = parse_headers(buffer, bytesCount);
            assert(headerSize <= bytesCount);
        }
        if (_finishParseHeaders) {
            if (!oldVal && bytesCount > headerSize) {
                // write remaining bytes to body
                _body.write(buffer + headerSize, bytesCount - headerSize);
                _totalRead += bytesCount - headerSize;
            } else if (oldVal) {
                _body.write(buffer, bytesCount);
                _totalRead += bytesCount;
            }
        }
        return _totalRead;
    }

    // for debug and logging
    std::string HTTPRequest::to_string(std::ostream &os) {
        libs::simple_format(os, "version: {}, method: {}, path: {}\r\n", _version, _method, _path);
        libs::simple_format(os, "headers: \n  {}\r\n", _headers);
        libs::simple_format(os, "query params: \n  {}\r\n", _queryParams);
        // std::string body = _body.str();
        // std::cout << "BODY: " << std::endl;
        // std::cout << body << std::endl;
        return {};// change to void?
    }

    std::string HTTPRequest::to_json(std::ostream &os) {
        // args: version, method, path, headers, query_parm
        auto jsonStrFormat = R"JSON({"version":"{}","method":"{}","path":"{}","headers":[{}],"query_params":[{}]})JSON";
        auto serialize_dict = [](const HeadersMap &dict) -> std::string {
            if (dict.empty())
                return "";
            std::string serializeStr = "{";
            for (const auto &ele : dict) {
                serializeStr += libs::simple_format(R"JSON("{}": "{}",)JSON", ele.first, ele.second);
            }
            serializeStr.pop_back();// remove last comma (,)
            serializeStr.push_back('}');
            return serializeStr;
        };
        libs::simple_format(os, jsonStrFormat,
                            _version, _method, _path, serialize_dict(_headers), serialize_dict(_queryParams));
        return {};// change to void?
    }

    HTTPResponse::HTTPResponse() : _version(HTTPVersion::HTTP_1_1),
                                   _statusCode(HTTPStatusCode::OK),
                                   _headers(),
                                   _body(),
                                   _inMemoryBody(),
                                   _readType(ReadType::UNINIT),
                                   _finishWriteHeader(false),
                                   _totalWrite(0),
                                   _contentLength(0) {}

    HTTPResponse::~HTTPResponse() {
        if (_body.is_open())
            _body.close();
        // if (fileFd_ > 0) ::close(fileFd_);
    }

    // HTTPResponse::HTTPResponse(HTTPResponse &&other) {
    //     _version = other._version;
    //     _statusCode = other._statusCode;
    //     _headers = other._headers;
    //     // _body = std::move(other._body);
    //     // _inMemoryBody = std::move(other._inMemoryBody);
    //     _readType = other._readType;
    //     _finishWriteHeader = other._finishWriteHeader;
    //     _totalWrite = other._totalWrite;
    //     _contentLength = other._contentLength;
    // }

    std::size_t HTTPResponse::serialize_header(char *buffer, std::size_t bufferSize) {
        // todo: change argument to buffer type; then write directly to buffer
        std::stringstream ss;
        libs::simple_format(ss, "{} {} {}\r\n", _version, static_cast<int>(_statusCode), _statusCode);
        if (_contentLength > 0)
            _headers["Content-Length"] = std::to_string(_contentLength);
        for (const auto &ele : _headers) {
            libs::simple_format(ss, "{}: {}\r\n", ele.first, ele.second);
        }
        ss << "\r\n";
        auto text = ss.str();
        auto headerSize = text.size();
        if (headerSize > bufferSize) {
            // todo: cho nay maybe co the allocate them memory
            throw std::logic_error("buffer to small");
            return headerSize;
        }
        text.copy(buffer, headerSize);
        _finishWriteHeader = true;
        return headerSize;
    }

    std::size_t HTTPResponse::serialize_body(std::istream &is, char *buffer, std::size_t bufferSize) {
        assert(_readType == ReadType::FILE_READ || _readType == ReadType::IN_MEMORY_READ);
        if (bufferSize <= 0) {// todo: remove this logic
            throw std::logic_error("buffer to small");
            return 0;
        }
        if (bufferSize > _contentLength) {
            // buffer is enough to write whole response
            is.read(buffer, _contentLength);
            _totalWrite = _contentLength;
            return _contentLength;
        } else {
            if (_contentLength - is.tellg() > bufferSize) {// first chunk or n-chunks
                is.read(buffer, bufferSize);
                _totalWrite += bufferSize;
                return bufferSize;
            } else {// last chunk
                auto lastBytes = _contentLength - is.tellg();
                is.read(buffer, lastBytes);
                _totalWrite += lastBytes;
                // _totalWrite = _contentLength;
                return lastBytes;
            }
        }
    }

    std::size_t HTTPResponse::serialize_reponse(char *buffer, std::size_t bufferSize) {
        std::size_t headerSize = 0;
        std::size_t bodySize = 0;
        if (!_finishWriteHeader) {
            headerSize = serialize_header(buffer, bufferSize);
            buffer += headerSize;
            bufferSize -= headerSize;
        }

        if (_readType == ReadType::FILE_READ) {
            bodySize = serialize_body(_body, buffer, bufferSize);
        } else if (_readType == ReadType::IN_MEMORY_READ) {
            bodySize = serialize_body(_inMemoryBody, buffer, bufferSize);
        } else {// empty body
        }
        return headerSize + bodySize;
    }

    // const BufferType HTTPResponse::write_header(MemBuf *membuf) {
    //     std::stringstream ss;
    //     libs::simple_format(ss, "{} {} {}\r\n", _version, static_cast<int>(_statusCode), _statusCode);
    //     if (_contentLength > 0)
    //         _headers["Content-Length"] = std::to_string(_contentLength);
    //     for (const auto &[key, value]: _headers) {
    //         libs::simple_format(ss, "{}: {}\r\n", key, value);
    //     }
    //     ss << "\r\n";
    //     std::string text(std::move(ss.str()));
    //     membuf->write(text.data(), text.size());
    //     _finishWriteHeader = true;
    //     BufferType ret;
    //     membuf->get_view(&ret.buf, &ret.len);
    //     return ret;
    // }

    // const BufferType HTTPResponse::write_body() {
    //     assert(_readType == ReadType::FILE_READ || _readType == ReadType::IN_MEMORY_READ);
    //     if (_readType == ReadType::IN_MEMORY_READ)
    //         return {memBody_.data(), memBody_.size()};
    //     assert(fileFd_ > 0);
    //     static thread_local char localBuf[4096]{};
    //     auto cnt = libs::read(fileFd_, fileOff_, localBuf, sizeof(localBuf));
    //     fileOff_ += cnt;
    //     return {localBuf, static_cast<size_t>(cnt)};
    // }

    // const BufferType HTTPResponse::write_response(MemBuf *membuf) {
    //     if (!_finishWriteHeader)
    //         return write_header(membuf);
    //     return write_body();
    // }

    bool HTTPResponse::writeDone() const {
        if (_contentLength == 0)
            return _finishWriteHeader;
        return _finishWriteHeader && (_totalWrite >= _contentLength);
    }

    void HTTPResponse::resetData() {
        _headers.clear();
        if (_readType == ReadType::IN_MEMORY_READ) {
            // memBody_.clear();
            _inMemoryBody.seekg(0, std::ios_base::beg);
            _inMemoryBody.seekp(0, std::ios_base::beg);
        } else if (_readType == ReadType::FILE_READ) {
            _body.close();
            // ::close(fileFd_);
            // fileOff_ = 0;
        }
        _readType = ReadType::UNINIT;
        _finishWriteHeader = false;
        _totalWrite = 0;
        _contentLength = 0;
    }

    void HTTPResponse::status_code(HTTPStatusCode statusCode) {
        _statusCode = statusCode;
    }

    void HTTPResponse::set_str_body(const std::string &content) {
        _readType = ReadType::IN_MEMORY_READ;
        // memBody_ = content;
        // _contentLength = content.size();
        _inMemoryBody << content;
        _contentLength = content.length();
    }

    void HTTPResponse::set_file_body(std::string path) {
        _readType = ReadType::FILE_READ;
        // fileFd_ = ::open(path.c_str(), O_RDONLY, 00644);
        // fileOff_ = 0;
        // struct ::stat st;
        // ::fstat(fileFd_, &st), 0;
        // _contentLength = st.st_size;
        _body.open(path);
        _contentLength = libs::content_length(_body);
    }

    void HTTPResponse::insert_header(std::pair<std::string, std::string> val) {
        _headers.insert(val);
    }

}// namespace simple_http

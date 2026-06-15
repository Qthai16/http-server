#include "HttpMessage.h"

#include "StrUtils.h"
#include "FileUtils.h"
#include "Defines.h"
#include "MemBuffer.h"

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

// todo: reject request if content length to large

#define MAX_MEM_BUFFER_SIZE     32768ul
#define MAX_BODY_SIZE           (8ul * 1024 * 1024)  // 8 MB hard limit per request body

namespace libs::http {

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

    std::string status_code_str(const HTTPCode &code) {
        switch (code) {
            case CODE_100: return "Continue";
            case CODE_200: return "OK";
            case CODE_201: return "Created";
            case CODE_202: return "Accepted";
            case CODE_203: return "Non-Authoritative Information";
            case CODE_204: return "No Content";
            case CODE_205: return "Reset Content";
            case CODE_206: return "Partial Content";
            case CODE_300: return "Multiple Choices";
            case CODE_301: return "Moved Permanently";
            case CODE_302: return "Found";
            case CODE_304: return "Not Modified";
            case CODE_400: return "Bad Request";
            case CODE_401: return "Unauthorized";
            case CODE_403: return "Forbidden";
            case CODE_404: return "Not Found";
            case CODE_405: return "Method Not Allowed";
            case CODE_408: return "Request Timeout";
            case CODE_418: return "I'm a Teapot";
            case CODE_500: return "Internal Server Error";
            case CODE_501: return "Not Implemented";
            case CODE_502: return "Bad Gateway";
            case CODE_503: return "Service Unavailable";
            case CODE_504: return "Gateway Timeout";
            case CODE_505: return "HTTP Version Not Supported";
            default:       return "";
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
    std::ostream &operator<<(std::ostream &os, HTTPCode code) {
        os << status_code_str(code);
        return os;
    }

    std::pair<bool, HTTPVersion> str_to_http_version(const std::string &str) {
        // std::string copyStr;
        // std::transform(str.cbegin(), str.cend(), std::back_inserter(copyStr), [](char c) {
        //     return std::toupper(c);
        // });
        if (strncmp(str.c_str(), "HTTP/", 5) != 0) {
            return {false, HTTPVersion::HTTP_1_1};
        }
        if (strncmp(str.c_str() + 5, "1.0", 3) == 0) {
            return {true, HTTPVersion::HTTP_1_0};
        } else if (strncmp(str.c_str() + 5, "1.1", 3) == 0) {
            return {true, HTTPVersion::HTTP_1_1};
        } else if (strncmp(str.c_str() + 5, "2.0", 3) == 0) {
            return {true, HTTPVersion::HTTP_2_0};
        }
        return {false, HTTPVersion::HTTP_1_1};
    }

    std::pair<bool, HTTPMethod> str_to_method(const std::string &str) {
        // static std::map<std::string, HTTPMethod> convertMap = {
        //         {"GET", HTTPMethod::GET},
        //         {"HEAD", HTTPMethod::HEAD},
        //         {"POST", HTTPMethod::POST},
        //         {"PUT", HTTPMethod::PUT},
        //         {"DELETE", HTTPMethod::DELETE},
        //         {"CONNECT", HTTPMethod::CONNECT},
        //         {"OPTIONS", HTTPMethod::OPTIONS},
        //         {"TRACE", HTTPMethod::TRACE},
        //         {"PATCH", HTTPMethod::PATCH},
        // };
        // std::string copyStr;
        // std::transform(str.cbegin(), str.cend(), std::back_inserter(copyStr), [](char c) {
        //     return std::toupper(c);
        // });
        // if (convertMap.count(copyStr))
        //     return convertMap.at(copyStr);
        if (strncmp(str.c_str(), "GET", 3) == 0) {
            return {true, HTTPMethod::GET};
        } else if (strncmp(str.c_str(), "HEAD", 4) == 0) {
            return {true, HTTPMethod::HEAD};
        } else if (strncmp(str.c_str(), "POST", 4) == 0) {
            return {true, HTTPMethod::POST};
        } else if (strncmp(str.c_str(), "PUT", 3) == 0) {
            return {true, HTTPMethod::PUT};
        } else if (strncmp(str.c_str(), "DELETE", 6) == 0) {
            return {true, HTTPMethod::DELETE};
        } else if (strncmp(str.c_str(), "CONNECT", 7) == 0) {
            return {true, HTTPMethod::CONNECT};
        } else if (strncmp(str.c_str(), "OPTIONS", 7) == 0) {
            return {true, HTTPMethod::OPTIONS};
        } else if (strncmp(str.c_str(), "TRACE", 5) == 0) {
            return {true, HTTPMethod::TRACE};
        } else if (strncmp(str.c_str(), "PATCH", 5) == 0) {
            return {true, HTTPMethod::PATCH};
        }
        return {false, HTTPMethod::GET};
    }

    std::string headers_get_field(const HeadersMap &headers, std::string key) {
        auto iter = std::find_if(headers.cbegin(), headers.cend(), [key](auto pair) {
            return libs::str_iequals(key, pair.first);
        });
        if (iter != headers.cend())
            return iter->second;
        return "";
    }

    HTTPRequest::HTTPRequest(size_t bufSize) : _version(HTTPVersion::HTTP_1_1),
                                 _method(HTTPMethod::GET),
                                 _headers(),
                                 _queryParams(),
                                 _path(),
                                 recv_buf_(bufSize),
                                 body_buf_(),
                                 _totalRead(0),
                                 _headerSize(0),
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
        try {
            return std::stoul(lenStr);
        } catch (const std::exception &) {
            return 0;
        }
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
        return _finishParseHeaders && (_totalRead - _headerSize >= _contentLength);
    }

    bool HTTPRequest::have_expect_continue() const {
        return _finishParseHeaders && _expectContinue;
    }

    void HTTPRequest::reset_expect_continue(bool val) {
        _expectContinue = val;
    }

    void HTTPRequest::resetData() {
        _version = HTTPVersion::HTTP_1_1;
        _method = HTTPMethod::GET;
        _headers.clear();
        _queryParams.clear();
        _path.clear();
        recv_buf_.resetBuf();
        body_buf_.resetBuf();
        _totalRead = 0;
        _headerSize = 0;
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
            if (size == 0) return 0;
            int i = 0, cnt = 0;
            do {
                if (buf[i] == '\n') {
                    cnt += 1;
                    return cnt;
                }
                out.push_back(buf[i]);
                cnt += 1;
            } while (++i < size);
            return cnt;
        };
        // first line is [method path version]
        cnt = getLine(buffer, bufsize, line);
        if (line.empty() || cnt == 0) return 0;
        else {
            auto tokens = libs::split_str(line, " ");
            if (tokens.size() != 3)
                throw std::runtime_error("invalid http message");
            auto methodRet = str_to_method(tokens[0]);
            if (!methodRet.first)
                throw std::runtime_error("invalid http method: " + tokens[0]);
            _method = methodRet.second;
            _path = tokens[1];
            parse_query_params(_path);
            auto versionRet = str_to_http_version(libs::trim(tokens[2]));
            if (!versionRet.first)
                throw std::runtime_error("invalid http version: " + tokens[2]);
            _version = versionRet.second;
        }
        totalCnt += cnt;
        buffer += cnt;
        bufsize -= cnt;
        // remain are headers
        bool foundTerminator = false;
        do {
            line.clear();
            cnt = getLine(buffer, bufsize, line);
            if (cnt == 0 || line.empty()) break;
            totalCnt += cnt;
            buffer += cnt;
            bufsize -= cnt;
            // parse header
            auto trimLine = libs::trim(line);
            if (trimLine.empty()) {
                foundTerminator = true;
                break;
            }
            auto delimPos = trimLine.find(':');
            if (delimPos == std::string::npos) continue;
            auto header = trimLine.substr(0, delimPos);
            auto tmp = trimLine.substr(delimPos + 1);
            auto value = libs::trim(tmp);
            _headers[header] = value;
        } while (bufsize > 0);
        if (foundTerminator) {
            _expectContinue = expect_100_continue();
            _contentLength = content_length();
            if (_contentLength > MAX_BODY_SIZE)
                throw std::runtime_error("request body too large");
            _finishParseHeaders = true;
        }
        return totalCnt;
    }

    libs::MemBuf* HTTPRequest::get_buf() {
        return &recv_buf_;
    }

    std::string HTTPRequest::body_str() const {
        return static_cast<std::string>(body_buf_);
    }

    std::size_t HTTPRequest::parse_request() {
        if (!_finishParseHeaders) {
            auto headerSize = parse_headers(recv_buf_.rd_pos(), recv_buf_.rd_avail());
            recv_buf_.incRdPos(headerSize);
            _totalRead += headerSize;
            if (_finishParseHeaders) {
                _headerSize = _totalRead;
                body_buf_.reserve(std::min(MAX_MEM_BUFFER_SIZE, _contentLength));
            }
        }
        if (_finishParseHeaders) {
            auto remaining = _contentLength - (_totalRead - _headerSize);
            auto len = std::min(recv_buf_.rd_avail(), remaining);
            body_buf_.write(recv_buf_.rd_pos(), len);
            recv_buf_.incRdPos(len);
            _totalRead += len;
        }
        if (recv_buf_.empty())
            recv_buf_.resetBuf();
        return _totalRead;
    }

    // for debug and logging
    std::string HTTPRequest::to_string(std::ostream &os) {
        libs::simple_format(os, "version: {}, method: {}, path: {}\r\n", _version, _method, _path);
        libs::simple_format(os, "headers: \n  {}\r\n", _headers);
        libs::simple_format(os, "query params: \n  {}\r\n", _queryParams);
        return {};// change to void?
    }

    std::string HTTPRequest::to_json(std::ostream &os) {
        // args: version, method, path, headers, query_parm
        auto jsonStrFormat = R"JSON({"version":"{}","method":"{}","path":"{}","headers":[{}],"query_params":[{}]})JSON";
        auto serialize_dict = [](const HeadersMap &dict) -> std::string {
            if (dict.empty())
                return "";
            std::string serializeStr = "{";
            for (const auto &ele: dict) {
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

    HTTPResponse::HTTPResponse(size_t bufsize)
        : _version(HTTPVersion::HTTP_1_1),
          _httpCode(HTTPCode::CODE_200),
          _headers(),
          buffer_{nullptr},
          memBody_{},
          fileFd_{-1},
          _readType(ReadType::UNINIT),
          _finishWriteHeader(false),
          _totalWrite(0),
          _contentLength(0) {
        init_buffer(bufsize);
    }

    HTTPResponse::~HTTPResponse() {
        if (fileFd_ >= 0) {
            ::close(fileFd_);
            fileFd_ = -1;
        }
    }

    HTTPResponse::HTTPResponse(HTTPResponse &&other) {
        _version = std::move(other._version);
        _httpCode = std::move(other._httpCode);
        _headers = std::move(other._headers);
        buffer_ = std::move(other.buffer_);
        memBody_ = std::move(other.memBody_);
        fileFd_ = other.fileFd_;
        other.fileFd_ = -1;
        wrOff_ = other.wrOff_;
        _readType = other._readType;
        _finishWriteHeader = other._finishWriteHeader;
        _totalWrite = other._totalWrite;
        _contentLength = other._contentLength;
    }

    HTTPResponse& HTTPResponse::operator=(HTTPResponse &&other) {
        if (this == &other) return *this;
        _version = std::move(other._version);
        _httpCode = std::move(other._httpCode);
        _headers = std::move(other._headers);
        buffer_ = std::move(other.buffer_);
        memBody_ = std::move(other.memBody_);
        fileFd_ = other.fileFd_;
        wrOff_ = other.wrOff_;
        _readType = other._readType;
        _finishWriteHeader = other._finishWriteHeader;
        _totalWrite = other._totalWrite;
        _contentLength = other._contentLength;

        other.resetData();
        other.buffer_.reset();
        return *this;
    }

    void HTTPResponse::write_reponse() {
        if (write_done()) return;
        if (!_finishWriteHeader)
            write_header();
        switch (_readType) {
            case ReadType::IN_MEMORY_READ: {
                write_mem_body();
            } break;
            case ReadType::FILE_READ: {
                write_file_body();
            } break;
            default:
                break;
        }
    }

    std::shared_ptr<BufferType> HTTPResponse::get_buf() const {
        return buffer_;
    }

    void HTTPResponse::init_buffer(size_t size) {
        buffer_.reset(new libs::MemBuf(size));
    }

    void HTTPResponse::write_header() {
        std::stringstream ss;
        libs::simple_format(ss, "{} {} {}\r\n", _version, static_cast<int>(_httpCode), _httpCode);
        if (_contentLength > 0)
            _headers["Content-Length"] = std::to_string(_contentLength);
        for (const auto &[key, value]: _headers) {
            libs::simple_format(ss, "{}: {}\r\n", key, value);
        }
        ss << "\r\n";
        std::string text(std::move(ss.str()));
        buffer_->reserve(std::min(MAX_MEM_BUFFER_SIZE, text.size() + _contentLength));
        buffer_->write(text.data(), text.size());
        _finishWriteHeader = true;
    }

    void HTTPResponse::write_mem_body() {
        // todo: copy for now, use view ownership with readv/writev later
        assert(_readType == ReadType::IN_MEMORY_READ);
        if (expr_unlikely(memBody_.empty())) return;
        size_t len = _contentLength - _totalWrite;
        if (len > buffer_->wr_avail())
            len = buffer_->wr_avail();
        buffer_->write(memBody_.data() + wrOff_, len);
        wrOff_ += len;
        _totalWrite += len;
    }

    void HTTPResponse::write_file_body() {
        assert(fileFd_ >= 0 && _readType == ReadType::FILE_READ);
        size_t len = _contentLength - _totalWrite;
        if (len > buffer_->wr_avail())
            len = buffer_->wr_avail();
        // todo: could use mmap file and map to directly page address to buffer
        libs::read(fileFd_, wrOff_, buffer_->wr_pos(), len);
        buffer_->incWrPos(len);
        wrOff_ += len;
        _totalWrite += len;
    }

    bool HTTPResponse::write_done() const {
        return _finishWriteHeader && (_contentLength > 0 ? _totalWrite >= _contentLength : 1);
    }

    void HTTPResponse::resetData() {
        _headers.clear();
        switch (_readType) {
            case ReadType::IN_MEMORY_READ: {
                memBody_.clear();
                wrOff_ = 0;
            } break;
            case ReadType::FILE_READ: {
                if (fileFd_ >= 0) {
                    ::close(fileFd_);
                    fileFd_ = -1;
                }
                wrOff_ = 0;
            } break;
            default:
                break;
        }
        if (buffer_) buffer_->resetBuf();
        _httpCode = HTTPCode::CODE_200;
        _version = HTTPVersion::HTTP_1_1;
        _readType = ReadType::UNINIT;
        _finishWriteHeader = false;
        _totalWrite = 0;
        _contentLength = 0;
    }

    void HTTPResponse::http_code(HTTPCode httpCode) {
        _httpCode = httpCode;
    }

    void HTTPResponse::str_body(const std::string &content) {
        if (expr_unlikely(content.empty())) return;
        _readType = ReadType::IN_MEMORY_READ;
        memBody_ = content;
        _contentLength = memBody_.size();
    }

    void HTTPResponse::str_body(const char *buf, size_t size) {
        if (expr_unlikely(size == 0)) return;
        _readType = ReadType::IN_MEMORY_READ;
        memBody_.assign(buf, size);
        _contentLength = memBody_.size();
    }

    void HTTPResponse::file_body(std::string path) {
        _readType = ReadType::FILE_READ;
        auto fd = open(path.c_str(), O_RDONLY, 0644);
        if (fd < 0) {
            // todo: set code 500 and return error msg
            throw std::runtime_error("failed to open stream");
        }
        fileFd_ = fd;
        // call fstat directly here instead of libs::file_size to avoid opening file again
        struct ::stat st;
        if (::fstat(fileFd_, &st) != 0)
            throw std::runtime_error("stat failed");
        _contentLength = st.st_size;
    }

    void HTTPResponse::insert_header(std::pair<std::string, std::string> val) {
        _headers.insert(val);
    }

}// namespace libs::http

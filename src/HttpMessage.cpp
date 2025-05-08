#include "HttpMessage.h"

using namespace HttpMessage;

// std::initializer_list<std::string> requiredHeaders = {"Content-Type", "Content-Length", "Connection"};

std::string HttpMessage::method_str(const HttpMessage::HTTPMethod &method) {
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

std::string HttpMessage::version_str(const HttpMessage::HTTPVersion &version) {
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

std::string HttpMessage::status_code_str(const HttpMessage::HTTPStatusCode &code) {
    switch (code) {
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

HttpMessage::HTTPVersion HttpMessage::str_to_http_version(const std::string &str) {
    static std::map<std::string, HttpMessage::HTTPVersion> convertMap = {
            {"HTTP/1.0", HttpMessage::HTTPVersion::HTTP_1_0},
            {"HTTP/1.1", HttpMessage::HTTPVersion::HTTP_1_1},
            {"HTTP/2.0", HttpMessage::HTTPVersion::HTTP_2_0}};
    std::string copyStr;
    std::transform(str.cbegin(), str.cend(), std::back_inserter(copyStr), [](char c) {
        return std::toupper(c);
    });
    if (convertMap.count(copyStr))
        return convertMap.at(copyStr);
    throw std::logic_error("HTTP version not support");
}

HttpMessage::HTTPMethod HttpMessage::str_to_method(const std::string &str) {
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
    if (convertMap.count(copyStr))
        return convertMap.at(copyStr);
    throw std::logic_error("method not support");
}

std::string HttpMessage::headers_get_field(const HttpMessage::HeadersMap &headers, std::string key) {
    auto iter = std::find_if(headers.cbegin(), headers.cend(), [key](auto pair) {
        return Utils::str_iequals(key, pair.first);
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
                             _bufferStream(),
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
    return Utils::str_iequals("100-continue", expectStatus);
}

bool HTTPRequest::request_completed() const {
    return _totalRead >= _contentLength;
}

void HTTPRequest::parse_query_params(const std::string &path) {
    auto startPos = path.find("?");
    if (startPos == std::string::npos)
        return;
    auto queries = path.substr(startPos + 1);
    auto pairs = Utils::split_str(queries, "&");
    for (const auto &pair: pairs) {
        auto delimPos = pair.find("=");
        if (delimPos != std::string::npos)
            _queryParams[pair.substr(0, delimPos)] = pair.substr(delimPos + 1);
    }
}

std::streampos HTTPRequest::parse_headers(std::istream &is) {
    std::string line;
    std::string firstLine;
    // first line is [method path version]
    if (std::getline(is, firstLine, '\n')) {
        auto tokens = Utils::split_str(firstLine, " ");
        if (tokens.size() != 3) {
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
    while (std::getline(is, line, '\n')) {
        auto trimLine = Utils::trim_str(line);
        if (trimLine.empty())
            break;
        auto delimPos = trimLine.find(':');
        auto header = trimLine.substr(0, delimPos);
        auto value = Utils::trim_str(trimLine.substr(delimPos + 1));
        _headers[header] = value;
    }
    _expectContinue = expect_100_continue();
    _contentLength = content_length();
    _finishParseHeaders = true;
    return is.tellg();
}

std::size_t HTTPRequest::parse_request(std::ostream &os, char *buffer, std::size_t bytesCount) {
    std::streampos headerSize = 0;
    if (!_finishParseHeaders) {
        std::stringstream tempBuffer;
        tempBuffer.write(buffer, bytesCount);
        headerSize = parse_headers(tempBuffer);
        // write remaining body bytes to os stream
        if (expect_100_continue())
            return _totalRead;
        auto wholeStr = tempBuffer.str();
        auto bodyPart = wholeStr.substr(headerSize);
        os.write(bodyPart.data(), bodyPart.size());
    } else {
        os.write(buffer, bytesCount);
    }
    _totalRead += bytesCount - headerSize;
    return _totalRead;
}

// for debug and logging
std::string HTTPRequest::to_string(std::ostream &os) {
    Utils::format_impl(os, "version: {}, method: {}, path: {}\r\n", version_str(_version), method_str(_method), _path);
    Utils::format_impl(os, "headers: [\r\n");
    for (const auto &[key, value]: _headers) {
        Utils::format_impl(os, "{}: {}\n", key, value);
    }
    Utils::format_impl(os, "]\r\n");

    Utils::format_impl(os, "query params: [\r\n");
    for (const auto &[key, value]: _queryParams) {
        Utils::format_impl(os, "{}: {}\n", key, value);
    }
    Utils::format_impl(os, "]\r\n");
    // std::string body = _body.str();
    // std::cout << "BODY: " << std::endl;
    // std::cout << body << std::endl;
    return "";// change to void?
}

std::string HTTPRequest::to_json(std::ostream &os) {
    // args: version, method, path, headers, query_parm
    auto jsonStrFormat = R"JSON({"version":"{}","method":"{}","path":"{}","headers":[{}],"query_params":[{}]})JSON";
    auto serialize_dict = [](const HeadersMap &dict) -> std::string {
        if (dict.empty())
            return "";
        std::string serializeStr = "{";
        for (const auto &[key, value]: dict) {
            serializeStr += Utils::simple_format(R"JSON("{}": "{}",)JSON", key, value);
        }
        serializeStr.pop_back();// remove last comma (,)
        serializeStr.push_back('}');
        return serializeStr;
    };
    Utils::format_impl(os, jsonStrFormat,
                       version_str(_version), method_str(_method), _path, serialize_dict(_headers), serialize_dict(_queryParams));
    return "";// change to void?
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
}

HTTPResponse::HTTPResponse(HTTPResponse &&other) {
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

std::size_t HTTPResponse::serialize_header(char *buffer, std::size_t bufferSize) {
    std::stringstream ss;
    Utils::format_impl(ss, "{} {} {}\r\n", version_str(_version), std::to_string(_statusCode), status_code_str(_statusCode));
    // ss << version_str(_version) << " " << std::to_string(_statusCode) << " " << status_code_str(_statusCode) << "\r\n";
    _headers["Content-Length"] = std::to_string(_contentLength);
    for (const auto &[key, value]: _headers) {
        // ss << key << ": " << value << "\r\n";
        Utils::format_impl(ss, "{}: {}\r\n", key, value);
    }
    ss << "\r\n";
    auto text = ss.str();
    auto headerSize = text.size();
    if (headerSize > bufferSize) {
        throw std::logic_error("buffer to small");
        return headerSize;
    }
    text.copy(buffer, headerSize);
    _finishWriteHeader = true;
    return headerSize;
}

std::size_t HTTPResponse::serialize_body(std::istream &is, char *buffer, std::size_t bufferSize, std::size_t headerSize) {
    auto remainBytes = bufferSize - headerSize;
    if (remainBytes <= 0) {
        throw std::logic_error("buffer to small");
        return 0;
    }
    if (remainBytes > _contentLength) {
        // buffer is enough to write whole response
        is.read(buffer + headerSize, _contentLength);
        _totalWrite = _contentLength;
        return _contentLength + headerSize;
    } else {
        if (remainBytes < bufferSize) {// first chunk
            is.read(buffer + headerSize, bufferSize - headerSize);
            _totalWrite += bufferSize - headerSize;
            return bufferSize;
        } else {
            if (_contentLength - is.tellg() > bufferSize) {
                is.read(buffer, bufferSize);
                _totalWrite += bufferSize;
                return bufferSize;
            } else {// last chunk
                auto lastBytes = _contentLength - is.tellg();
                is.read(buffer, lastBytes);
                _totalWrite = _contentLength;
                return lastBytes;
            }
        }
    }
}

std::size_t HTTPResponse::serialize_reponse(char *buffer, std::size_t bufferSize) {
    std::size_t headerSize = 0;
    std::size_t writeCount = 0;
    if (!_finishWriteHeader)
        headerSize = serialize_header(buffer, bufferSize);

    if (_readType == ReadType::FILE_READ) {
        writeCount = serialize_body(_body, buffer, bufferSize, headerSize);
    } else if (_readType == ReadType::IN_MEMORY_READ) {
        writeCount = serialize_body(_inMemoryBody, buffer, bufferSize, headerSize);
    } else {// empty body?
    }
    return writeCount;
}

void HTTPResponse::status_code(HTTPStatusCode statusCode) {
    _statusCode = statusCode;
}

void HTTPResponse::set_str_body(const std::string &content) {
    _readType = ReadType::IN_MEMORY_READ;
    _inMemoryBody << content;
    _contentLength = content.length();
}

void HTTPResponse::set_file_body(std::string path) {
    _readType = ReadType::FILE_READ;
    _body.open(path);
    _contentLength = Utils::content_length(_body);
}

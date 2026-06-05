#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <unistd.h>
#include "libs/http/HttpMessage.h"

using namespace libs::http;

namespace {

void feedChunk(HTTPRequest &req, std::string_view data) {
    req.get_buf()->write(data.data(), data.size());
    req.parse_request();
}

std::string drainResponse(HTTPResponse &res) {
    std::string out;
    do {
        res.write_reponse();
        auto buf = res.get_buf();
        out.append(buf->rd_pos(), buf->rd_avail());
        buf->resetBuf();
    } while (!res.write_done());
    return out;
}

std::string createTempFile(const std::string &content) {
    char tmp[] = "/tmp/httptest_XXXXXX";
    int fd = mkstemp(tmp);
    ::write(fd, content.data(), content.size());
    ::close(fd);
    return std::string(tmp);
}

} // namespace

// ---- Helper function roundtrips ----

TEST(http_helpers, method_str_roundtrip) {
    using M = HTTPMethod;
    std::vector<std::pair<std::string, M>> cases = {
        {"GET", M::GET}, {"HEAD", M::HEAD}, {"POST", M::POST}, {"PUT", M::PUT},
        {"DELETE", M::DELETE}, {"CONNECT", M::CONNECT},
        {"OPTIONS", M::OPTIONS}, {"TRACE", M::TRACE}, {"PATCH", M::PATCH},
    };
    for (auto &[name, method] : cases) {
        EXPECT_EQ(method_str(method), name) << name;
        auto [ok, m] = str_to_method(name);
        EXPECT_TRUE(ok) << name;
        EXPECT_EQ(m, method) << name;
    }
}

TEST(http_helpers, str_to_method_invalid) {
    EXPECT_FALSE(str_to_method("BADMETHOD").first);
    EXPECT_FALSE(str_to_method("").first);
    EXPECT_FALSE(str_to_method("get").first); // case-sensitive
}

TEST(http_helpers, version_str_roundtrip) {
    auto [ok10, v10] = str_to_http_version("HTTP/1.0");
    EXPECT_TRUE(ok10);
    EXPECT_EQ(version_str(v10), "HTTP/1.0");

    auto [ok11, v11] = str_to_http_version("HTTP/1.1");
    EXPECT_TRUE(ok11);
    EXPECT_EQ(version_str(v11), "HTTP/1.1");

    auto [ok20, v20] = str_to_http_version("HTTP/2.0");
    EXPECT_TRUE(ok20);
    EXPECT_EQ(version_str(v20), "HTTP/2.0");
}

TEST(http_helpers, str_to_http_version_invalid) {
    EXPECT_FALSE(str_to_http_version("HTTP/9.9").first);
    EXPECT_FALSE(str_to_http_version("NOTHTTP/1.1").first);
    EXPECT_FALSE(str_to_http_version("").first);
}

TEST(http_helpers, status_code_str) {
    EXPECT_EQ(status_code_str(CODE_200), "OK");
    EXPECT_EQ(status_code_str(CODE_201), "Created");
    EXPECT_EQ(status_code_str(CODE_204), "No Content");
    EXPECT_EQ(status_code_str(CODE_301), "Moved Permanently");
    EXPECT_EQ(status_code_str(CODE_400), "Bad Request");
    EXPECT_EQ(status_code_str(CODE_401), "Unauthorized");
    EXPECT_EQ(status_code_str(CODE_404), "Not Found");
    EXPECT_EQ(status_code_str(CODE_418), "I'm a Teapot");
    EXPECT_EQ(status_code_str(CODE_500), "Internal Server Error");
    EXPECT_EQ(status_code_str(CODE_503), "Service Unavailable");
}

TEST(http_helpers, headers_get_field_case_insensitive) {
    HeadersMap h = {{"Content-Type", "text/plain"}, {"X-Custom", "val"}};
    EXPECT_EQ(headers_get_field(h, "content-type"), "text/plain");
    EXPECT_EQ(headers_get_field(h, "CONTENT-TYPE"), "text/plain");
    EXPECT_EQ(headers_get_field(h, "x-custom"), "val");
    EXPECT_EQ(headers_get_field(h, "Missing"), "");
}

// ---- HTTPRequest parsing: valid cases ----

TEST(http_request_parsing, get_no_body) {
    HTTPRequest req(4096);
    feedChunk(req, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_EQ(req._method, HTTPMethod::GET);
    EXPECT_EQ(req._path, "/");
    EXPECT_EQ(req._version, HTTPVersion::HTTP_1_1);
    EXPECT_TRUE(req.request_completed());
    EXPECT_EQ(req.content_length(), 0u);
    EXPECT_EQ(req.body_str(), "");
}

TEST(http_request_parsing, get_http_1_0) {
    HTTPRequest req(4096);
    feedChunk(req, "GET /page HTTP/1.0\r\n\r\n");
    EXPECT_EQ(req._version, HTTPVersion::HTTP_1_0);
    EXPECT_EQ(req._path, "/page");
    EXPECT_TRUE(req.request_completed());
}

TEST(http_request_parsing, post_with_body) {
    HTTPRequest req(4096);
    feedChunk(req, "POST /submit HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello");
    EXPECT_EQ(req._method, HTTPMethod::POST);
    EXPECT_EQ(req._path, "/submit");
    EXPECT_EQ(req.content_length(), 5u);
    EXPECT_EQ(req.body_str(), "hello");
    EXPECT_TRUE(req.request_completed());
}

TEST(http_request_parsing, all_methods) {
    std::vector<std::pair<std::string, HTTPMethod>> cases = {
        {"GET", HTTPMethod::GET}, {"HEAD", HTTPMethod::HEAD},
        {"POST", HTTPMethod::POST}, {"PUT", HTTPMethod::PUT},
        {"DELETE", HTTPMethod::DELETE}, {"CONNECT", HTTPMethod::CONNECT},
        {"OPTIONS", HTTPMethod::OPTIONS}, {"TRACE", HTTPMethod::TRACE},
        {"PATCH", HTTPMethod::PATCH},
    };
    for (auto &[name, expected] : cases) {
        HTTPRequest req(4096);
        feedChunk(req, name + " / HTTP/1.1\r\n\r\n");
        EXPECT_EQ(req._method, expected) << name;
    }
}

TEST(http_request_parsing, multiple_headers_stored) {
    HTTPRequest req(4096);
    feedChunk(req, "GET / HTTP/1.1\r\nHost: example.com\r\nAccept: */*\r\n"
                   "X-Custom: myvalue\r\nContent-Type: text/plain\r\n\r\n");
    EXPECT_EQ(req.get_header("Host"), "example.com");
    EXPECT_EQ(req.get_header("Accept"), "*/*");
    EXPECT_EQ(req.get_header("X-Custom"), "myvalue");
    EXPECT_EQ(req.get_header("Content-Type"), "text/plain");
}

TEST(http_request_parsing, content_length_zero_completes_immediately) {
    HTTPRequest req(4096);
    feedChunk(req, "POST /upload HTTP/1.1\r\nContent-Length: 0\r\n\r\n");
    EXPECT_EQ(req.content_length(), 0u);
    EXPECT_TRUE(req.request_completed());
    EXPECT_EQ(req.body_str(), "");
}

TEST(http_request_parsing, expect_100_continue) {
    HTTPRequest req(4096);
    feedChunk(req, "POST /upload HTTP/1.1\r\nContent-Length: 100\r\nExpect: 100-continue\r\n\r\n");
    EXPECT_TRUE(req.expect_100_continue());
    EXPECT_TRUE(req.have_expect_continue());
    EXPECT_FALSE(req.request_completed()); // body not yet received
}

// ---- HTTPRequest parsing: error cases ----

TEST(http_request_parsing, invalid_method_throws) {
    HTTPRequest req(4096);
    EXPECT_THROW(feedChunk(req, "BADMETHOD / HTTP/1.1\r\n\r\n"), std::runtime_error);
}

TEST(http_request_parsing, invalid_version_throws) {
    HTTPRequest req(4096);
    EXPECT_THROW(feedChunk(req, "GET / HTTP/9.9\r\n\r\n"), std::runtime_error);
}

TEST(http_request_parsing, malformed_first_line_two_tokens_throws) {
    HTTPRequest req(4096);
    EXPECT_THROW(feedChunk(req, "GET /\r\n\r\n"), std::runtime_error);
}

TEST(http_request_parsing, body_too_large_throws) {
    HTTPRequest req(4096);
    // 9 MB > MAX_BODY_SIZE (8 MB)
    std::string raw = "POST /up HTTP/1.1\r\nContent-Length: " +
                      std::to_string(9ul * 1024 * 1024) + "\r\n\r\n";
    EXPECT_THROW(feedChunk(req, raw), std::runtime_error);
}

// ---- HTTPRequest: partial / multi-call body parsing ----

TEST(http_request_parsing, body_arrives_in_two_chunks) {
    HTTPRequest req(4096);
    feedChunk(req, "POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\nhel");
    EXPECT_FALSE(req.request_completed());
    EXPECT_EQ(req.body_str(), "hel");
    feedChunk(req, "lo");
    EXPECT_TRUE(req.request_completed());
    EXPECT_EQ(req.body_str(), "hello");
}

TEST(http_request_parsing, body_arrives_in_three_chunks) {
    HTTPRequest req(4096);
    feedChunk(req, "POST / HTTP/1.1\r\nContent-Length: 9\r\n\r\nabc");
    EXPECT_FALSE(req.request_completed());
    feedChunk(req, "def");
    EXPECT_FALSE(req.request_completed());
    feedChunk(req, "ghi");
    EXPECT_TRUE(req.request_completed());
    EXPECT_EQ(req.body_str(), "abcdefghi");
}

// ---- HTTPRequest: header accessors ----

TEST(http_request_parsing, get_header_case_insensitive) {
    HTTPRequest req(4096);
    feedChunk(req, "GET / HTTP/1.1\r\nContent-Type: text/html\r\n\r\n");
    EXPECT_EQ(req.get_header("Content-Type"), "text/html");
    EXPECT_EQ(req.get_header("content-type"), "text/html");
    EXPECT_EQ(req.get_header("CONTENT-TYPE"), "text/html");
}

TEST(http_request_parsing, get_header_missing_returns_empty) {
    HTTPRequest req(4096);
    feedChunk(req, "GET / HTTP/1.1\r\n\r\n");
    EXPECT_EQ(req.get_header("X-Nonexistent"), "");
}

TEST(http_request_parsing, content_filename_extracted) {
    HTTPRequest req(4096);
    feedChunk(req, "POST /upload HTTP/1.1\r\n"
                   "Content-Disposition: attachment; filename=\"photo.jpg\"\r\n\r\n");
    EXPECT_EQ(req.content_filename(), "photo.jpg");
}

TEST(http_request_parsing, content_filename_missing_returns_empty) {
    HTTPRequest req(4096);
    feedChunk(req, "GET / HTTP/1.1\r\n\r\n");
    EXPECT_EQ(req.content_filename(), "");
}

// ---- HTTPRequest: query parameter parsing ----

TEST(http_request_query_params, single_param) {
    HTTPRequest req(4096);
    req.parse_query_params("/path?key=val");
    EXPECT_EQ(req._queryParams["key"], "val");
}

TEST(http_request_query_params, multiple_params) {
    HTTPRequest req(4096);
    req.parse_query_params("/path?a=1&b=2&c=3");
    EXPECT_EQ(req._queryParams["a"], "1");
    EXPECT_EQ(req._queryParams["b"], "2");
    EXPECT_EQ(req._queryParams["c"], "3");
}

TEST(http_request_query_params, no_params_empty_map) {
    HTTPRequest req(4096);
    req.parse_query_params("/path");
    EXPECT_TRUE(req._queryParams.empty());
}

TEST(http_request_query_params, empty_value) {
    HTTPRequest req(4096);
    req.parse_query_params("/path?key=");
    EXPECT_EQ(req._queryParams["key"], "");
}

TEST(http_request_query_params, malformed_no_equals_skipped) {
    HTTPRequest req(4096);
    req.parse_query_params("/path?noequals");
    EXPECT_TRUE(req._queryParams.empty());
}

TEST(http_request_query_params, parsed_from_request_line) {
    HTTPRequest req(4096);
    feedChunk(req, "GET /search?q=hello&lang=en HTTP/1.1\r\n\r\n");
    EXPECT_EQ(req._queryParams["q"], "hello");
    EXPECT_EQ(req._queryParams["lang"], "en");
}

// ---- HTTPRequest: resetData ----

TEST(http_request_parsing, reset_data_clears_all_state) {
    HTTPRequest req(4096);
    feedChunk(req, "POST /foo HTTP/1.1\r\nContent-Length: 3\r\n\r\nabc");
    ASSERT_TRUE(req.request_completed());

    req.resetData();
    EXPECT_EQ(req._totalRead, 0u);
    EXPECT_FALSE(req._finishParseHeaders);
    EXPECT_TRUE(req._headers.empty());
    EXPECT_TRUE(req._path.empty());
    EXPECT_EQ(req.body_str(), "");
    EXPECT_EQ(req._contentLength, 0u);
}

TEST(http_request_parsing, reuse_after_reset) {
    HTTPRequest req(4096);
    feedChunk(req, "GET / HTTP/1.1\r\n\r\n");
    ASSERT_EQ(req._path, "/");
    req.resetData();

    feedChunk(req, "POST /submit HTTP/1.1\r\nContent-Length: 2\r\n\r\nhi");
    EXPECT_EQ(req._method, HTTPMethod::POST);
    EXPECT_EQ(req._path, "/submit");
    EXPECT_EQ(req.body_str(), "hi");
    EXPECT_TRUE(req.request_completed());
}

// ---- HTTPResponse: write_reponse and str_body ----

TEST(http_response_write, status_line_format) {
    HTTPResponse res(4096);
    res.http_code(CODE_200);
    std::string raw = drainResponse(res);
    EXPECT_EQ(raw.find("HTTP/1.1 200 OK\r\n"), 0u);
}

TEST(http_response_write, custom_header_in_output) {
    HTTPResponse res(4096);
    res.http_code(CODE_200);
    res.insert_header({"X-Foo", "bar"});
    std::string raw = drainResponse(res);
    EXPECT_NE(raw.find("X-Foo: bar\r\n"), std::string::npos);
}

TEST(http_response_write, content_length_auto_added_for_body) {
    HTTPResponse res(4096);
    res.str_body("hello");
    std::string raw = drainResponse(res);
    EXPECT_NE(raw.find("Content-Length: 5\r\n"), std::string::npos);
}

TEST(http_response_write, body_appears_after_header_terminator) {
    HTTPResponse res(4096);
    res.str_body("hello");
    std::string raw = drainResponse(res);
    auto sep = raw.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    EXPECT_EQ(raw.substr(sep + 4), "hello");
}

TEST(http_response_write, empty_body_no_content_length) {
    HTTPResponse res(4096);
    res.http_code(CODE_204);
    std::string raw = drainResponse(res);
    EXPECT_EQ(raw.find("Content-Length"), std::string::npos);
    EXPECT_TRUE(res.write_done());
}

TEST(http_response_write, write_done_true_after_drain) {
    HTTPResponse res(4096);
    res.str_body("data");
    drainResponse(res);
    EXPECT_TRUE(res.write_done());
}

TEST(http_response_write, all_status_codes) {
    struct { HTTPCode code; const char *expected; } cases[] = {
        {CODE_201, "201 Created"},
        {CODE_301, "301 Moved Permanently"},
        {CODE_400, "400 Bad Request"},
        {CODE_404, "404 Not Found"},
        {CODE_500, "500 Internal Server Error"},
        {CODE_503, "503 Service Unavailable"},
    };
    for (auto &c : cases) {
        HTTPResponse res(4096);
        res.http_code(c.code);
        std::string raw = drainResponse(res);
        EXPECT_NE(raw.find(c.expected), std::string::npos) << c.expected;
    }
}

TEST(http_response_write, large_body_piecemeal_correct) {
    // 64KB body exceeds MAX_MEM_BUFFER_SIZE (32KB), requiring multiple writes
    std::string body(65536, 'X');
    HTTPResponse res(4096);
    res.str_body(body);
    std::string raw = drainResponse(res);
    auto sep = raw.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    EXPECT_EQ(raw.substr(sep + 4), body);
}

// ---- HTTPResponse: file_body ----

TEST(http_response_file_body, content_read_correctly) {
    std::string path = createTempFile("abc\n");
    HTTPResponse res(4096);
    res.file_body(path);
    std::string raw = drainResponse(res);
    auto sep = raw.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    EXPECT_EQ(raw.substr(sep + 4), "abc\n");
    ::unlink(path.c_str());
}

TEST(http_response_file_body, content_length_header_matches_file_size) {
    std::string content = "hello file";
    std::string path = createTempFile(content);
    HTTPResponse res(4096);
    res.file_body(path);
    std::string raw = drainResponse(res);
    std::string expected_cl = "Content-Length: " + std::to_string(content.size()) + "\r\n";
    EXPECT_NE(raw.find(expected_cl), std::string::npos);
    ::unlink(path.c_str());
}

TEST(http_response_file_body, nonexistent_file_throws) {
    HTTPResponse res(4096);
    EXPECT_THROW(res.file_body("/nonexistent/path/file.txt"), std::runtime_error);
}

TEST(http_response_file_body, large_file_piecemeal_correct) {
    std::string content(100 * 1024, 'Z'); // 100KB
    std::string path = createTempFile(content);
    HTTPResponse res(4096);
    res.file_body(path);
    std::string raw = drainResponse(res);
    auto sep = raw.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    EXPECT_EQ(raw.substr(sep + 4), content);
    ::unlink(path.c_str());
}

// ---- HTTPResponse: move semantics ----

TEST(http_response_move, move_constructor_transfers_ownership) {
    HTTPResponse r1(4096);
    r1.str_body("hi");
    HTTPResponse r2(std::move(r1));
    std::string raw = drainResponse(r2);
    auto sep = raw.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    EXPECT_EQ(raw.substr(sep + 4), "hi");
}

TEST(http_response_move, move_assignment_transfers_ownership) {
    HTTPResponse r1(4096);
    r1.str_body("world");
    HTTPResponse r2(4096);
    r2 = std::move(r1);
    std::string raw = drainResponse(r2);
    auto sep = raw.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    EXPECT_EQ(raw.substr(sep + 4), "world");
}

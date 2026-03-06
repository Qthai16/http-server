#include <gtest/gtest.h>
#include "libs/http/HttpMessage.h"

TEST(http_message, parse_request) {
    auto g_test_GET_request = R"TEST(GET / HTTP/1.1
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
Accept-Encoding: gzip, deflate
Accept-Language: en-US,en;q=0.9
Cache-Control: max-age=0
Connection: keep-alive
Host: 172.31.234.35:11225
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36

)TEST";

    auto g_test_POST_request = R"TEST(POST /form HTTP/1.1
Host: localhost:11225
User-Agent: curl/7.68.0
Accept: */*
Content-Length: 4391168
Content-Type: application/x-www-form-urlencoded
Expect: 100-continue)TEST";

    auto g_test_requests = {g_test_GET_request, g_test_POST_request};
    // todo define http message parser test
    EXPECT_TRUE(true);
}

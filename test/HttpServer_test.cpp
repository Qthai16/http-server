#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <vector>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "libs/http/HttpMessage.h"
#include "libs/http/SimpleServer.h"
#include "libs/EzCurl.h"

using namespace libs::http;

// Each process seeds from its PID so parallel ctest invocations don't share ports.
// Port range: [20000, 29999] — well outside ephemeral range and server defaults.
static unsigned int nextPort() {
    static std::atomic<unsigned int> counter{
        20000u + static_cast<unsigned int>(getpid()) % 10000u
    };
    return counter.fetch_add(1, std::memory_order_relaxed);
}

namespace {

std::string url(unsigned int port, const char *path) {
    return "http://127.0.0.1:" + std::to_string(port) + path;
}

std::string createTempFile(const std::string &content) {
    char tmp[] = "/tmp/httpsvr_XXXXXX";
    int fd = mkstemp(tmp);
    ::write(fd, content.data(), content.size());
    ::close(fd);
    return std::string(tmp);
}

// RAII server fixture: start on construction, stop on destruction
struct TestServer {
    unsigned int port;
    std::shared_ptr<SimpleServer> server;

    TestServer() : port(nextPort()),
                   server(std::make_shared<SimpleServer>("0.0.0.0", port, 2)) {}

    void start() {
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ~TestServer() {
        server->stop();
        server.reset();
    }

    std::string url(const char *path) const {
        return "http://127.0.0.1:" + std::to_string(port) + path;
    }
};

} // namespace

TEST(http_server, start_stop) {
    auto server = std::make_shared<SimpleServer>("0.0.0.0", nextPort(), 4);
    server->start();
    server->stop();
    server.reset();
    EXPECT_TRUE(true);
}

TEST(http_server, get_returns_200_and_body) {
    TestServer s;
    s.server->addHandler("/hello", HTTPMethod::GET, [](HTTPRequest *, HTTPResponse *res) {
        res->http_code(CODE_200);
        res->str_body("hello");
    });
    s.start();

    libs::CurlWrapper client;
    auto [code, body] = client.Get(s.url("/hello"));
    EXPECT_EQ(code, 200);
    EXPECT_EQ(body, "hello");
}

TEST(http_server, post_echo_body) {
    TestServer s;
    s.server->addHandler("/echo", HTTPMethod::POST, [](HTTPRequest *req, HTTPResponse *res) {
        res->http_code(CODE_200);
        res->str_body(req->body_str());
    });
    s.start();

    libs::CurlWrapper client;
    auto [code, body] = client.Post(s.url("/echo"), "testdata");
    EXPECT_EQ(code, 200);
    EXPECT_EQ(body, "testdata");
}

TEST(http_server, unregistered_path_returns_404) {
    TestServer s;
    s.server->setDefaultHandler(HTTPMethod::GET, [](HTTPRequest *, HTTPResponse *res) {
        res->http_code(CODE_404);
        res->str_body("not found");
    });
    s.start();

    libs::CurlWrapper client;
    auto [code, body] = client.Get(s.url("/nonexistent"));
    EXPECT_EQ(code, 404);
}

TEST(http_server, query_params_received_in_handler) {
    TestServer s;
    // regex_match sees full path including query string; use pattern to accept optional query
    s.server->addHandler("/query(\\?.*)?", HTTPMethod::GET,
                         [](HTTPRequest *req, HTTPResponse *res) {
                             res->http_code(CODE_200);
                             auto it = req->_queryParams.find("key");
                             res->str_body(it != req->_queryParams.end() ? it->second : "");
                         });
    s.start();

    libs::CurlWrapper client;
    auto [code, body] = client.Get(s.url("/query?key=testval"));
    EXPECT_EQ(code, 200);
    EXPECT_EQ(body, "testval");
}

TEST(http_server, static_file_serving) {
    std::string content = "file contents here\n";
    std::string path = createTempFile(content);
    TestServer s;
    s.server->addHandler("/file", HTTPMethod::GET, [path](HTTPRequest *, HTTPResponse *res) {
        res->http_code(CODE_200);
        res->file_body(path);
    });
    s.start();

    libs::CurlWrapper client;
    auto [code, body] = client.Get(s.url("/file"));
    EXPECT_EQ(code, 200);
    EXPECT_EQ(body, content);
    ::unlink(path.c_str());
}

TEST(http_server, multiple_sequential_requests) {
    TestServer s;
    s.server->addHandler("/hello", HTTPMethod::GET, [](HTTPRequest *, HTTPResponse *res) {
        res->http_code(CODE_200);
        res->str_body("hello");
    });
    s.start();

    libs::CurlWrapper client;
    for (int i = 0; i < 5; ++i) {
        auto [code, body] = client.Get(s.url("/hello"));
        EXPECT_EQ(code, 200) << "request " << i;
        EXPECT_EQ(body, "hello") << "request " << i;
    }
}

TEST(http_server, concurrent_requests) {
    TestServer s;
    s.server->addHandler("/hello", HTTPMethod::GET, [](HTTPRequest *, HTTPResponse *res) {
        res->http_code(CODE_200);
        res->str_body("hello");
    });
    s.start();

    constexpr int N_THREADS = 10;
    constexpr int N_REQS = 5;
    std::atomic_int successes{0};
    std::vector<std::thread> threads;
    threads.reserve(N_THREADS);

    unsigned int port = s.port;
    for (int t = 0; t < N_THREADS; ++t) {
        threads.emplace_back([&successes, port]() {
            libs::CurlWrapper client;
            for (int i = 0; i < N_REQS; ++i) {
                auto [code, body] = client.Get(url(port, "/hello"));
                if (code == 200 && body == "hello")
                    successes.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto &t : threads)
        t.join();
    EXPECT_EQ(successes.load(), N_THREADS * N_REQS);
}

TEST(http_server, large_post_body_echoed) {
    TestServer s;
    s.server->addHandler("/echo", HTTPMethod::POST, [](HTTPRequest *req, HTTPResponse *res) {
        res->http_code(CODE_200);
        res->str_body(req->body_str());
    });
    s.start();

    std::string large_body(64 * 1024, 'A'); // 64KB
    libs::CurlWrapper client;
    auto [code, body] = client.Post(s.url("/echo"), large_body);
    EXPECT_EQ(code, 200);
    EXPECT_EQ(body, large_body);
}

// Returns a connected TCP socket to 127.0.0.1:port, or -1 on failure.
static int rawConnect(unsigned int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

// Blocks until recv returns 0 (server closed) or times out. Returns true if the
// server closed the connection within the given deadline.
static bool waitForServerClose(int fd, std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    char buf[1];
    while (std::chrono::steady_clock::now() < deadline) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) break;
        struct timeval tv{static_cast<time_t>(remaining.count() / 1'000'000),
                          static_cast<suseconds_t>(remaining.count() % 1'000'000)};
        int rv = ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
        if (rv > 0) {
            auto n = ::recv(fd, buf, 1, MSG_DONTWAIT);
            if (n == 0) return true;  // FIN received — server closed
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return true;
        }
    }
    return false;
}

TEST(http_server, read_timeout_closes_idle_connection) {
    TestServer s;
    s.server->setReadTimeout(std::chrono::seconds{2});
    s.server->setIdleTimeout(std::chrono::seconds{60});
    s.start();

    int fd = rawConnect(s.port);
    ASSERT_GE(fd, 0);

    // Do not send anything — server should close the connection after ~2 s.
    bool closed = waitForServerClose(fd, std::chrono::milliseconds{3500});
    ::close(fd);
    EXPECT_TRUE(closed) << "server did not close idle connection within read timeout";
}

TEST(http_server, idle_timeout_closes_keepalive_connection) {
    TestServer s;
    s.server->setReadTimeout(std::chrono::seconds{30});
    s.server->setIdleTimeout(std::chrono::seconds{2});
    s.server->addHandler("/ping", HTTPMethod::GET, [](HTTPRequest *, HTTPResponse *res) {
        res->http_code(CODE_200);
        res->str_body("pong");
    });
    s.start();

    // Complete one request so the connection enters the idle (keep-alive) state.
    libs::CurlWrapper client;
    auto [code, body] = client.Get(s.url("/ping"));
    ASSERT_EQ(code, 200);

    // Now open a raw connection, send a full request to get the keep-alive idle state,
    // then hold the connection open — server should close it after the idle timeout.
    int fd = rawConnect(s.port);
    ASSERT_GE(fd, 0);

    const char *req = "GET /ping HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";
    ::send(fd, req, strlen(req), MSG_NOSIGNAL);

    // Drain the response so the connection goes idle.
    char buf[4096];
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds{2}) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv{0, 50'000};
        if (::select(fd + 1, &rfds, nullptr, nullptr, &tv) > 0) {
            int n = ::recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
            if (n <= 0) break;
        }
    }

    // Connection is now idle — wait for server to close it via idle timeout (~2 s).
    bool closed = waitForServerClose(fd, std::chrono::milliseconds{4000});
    ::close(fd);
    EXPECT_TRUE(closed) << "server did not close keep-alive connection within idle timeout";
}

TEST(http_server, server_stats_after_requests) {
    TestServer s;
    s.server->addHandler("/hello", HTTPMethod::GET, [](HTTPRequest *, HTTPResponse *res) {
        res->http_code(CODE_200);
        res->str_body("hello");
    });
    s.start();

    libs::CurlWrapper client;
    for (int i = 0; i < 3; ++i)
        client.Get(s.url("/hello"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto stats = s.server->getStat();
    EXPECT_GE(stats.successReq, 3u);
    EXPECT_EQ(stats.failedReq, 0);
}

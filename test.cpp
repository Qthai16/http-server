#include <iostream>
#include <string>
#include <thread>

#include <cassert>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <netinet/in.h>
#include <regex>
#include <sstream>
#include <tuple>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include <chrono>
#if __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include "src/HttpMessage.h"
#ifdef USE_OPENSSL
#include "src/SSLUtils.h"
#endif
#include "src/SimpleServer.h"

#include "libs/StrUtils.h"

using namespace std::placeholders;
using namespace simple_http;

// #define THREADPOOL_SIZE 4

#define Q(x)                      #x
#define ASSERT_EQ(first, second)  assert(first == second)
#define ASSERT_NEQ(first, second) assert(first != second)

static void SendStaticFile(std::string path, HTTPRequest *req, HTTPResponse *response) {
    // response->_version = req._version;
    response->insert_header({"Connection", "keep-alive"});

    if (!fs::exists(path)) {
        // send 404 not found
        std::string sendData = R"JSON({"errors": "resource not found"})JSON";
        response->status_code(HTTPStatusCode::NotFound);
        response->set_str_body(sendData);
        response->insert_header({"Content-Type", "application/json"});
        return;
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        // send internal error
        std::string sendData = R"JSON({"errors": "failed to open file"})JSON";
        response->status_code(HTTPStatusCode::InternalServerError);
        response->set_str_body(sendData);
        response->insert_header({"Content-Type", "application/json"});
        return;
    }
    file.close();// close here bc HTTPResponse will open this file

    auto extension = fs::path(path).extension().string();
    if (simple_http::_mimeTypes.count(extension)) {
        response->insert_header({"Content-Type", simple_http::_mimeTypes.at(extension)});
    } else {
        response->insert_header({"Content-Type", "application/octet-stream"});// default for others
    }
    response->status_code(HTTPStatusCode::OK);
    response->set_file_body(path);
}

static SimpleServer::HandlersMap ServeStaticResources(std::string rootPath) {
    SimpleServer::HandlersMap handlersMap;
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        return handlersMap;
    }
    for (auto &entry: fs::recursive_directory_iterator(rootPath)) {
        if (!fs::is_regular_file(entry.path()))
            continue;
        auto filePath = entry.path();
        auto urlPath = filePath.string().substr(rootPath.length());
        auto absolutePath = filePath.string();
        // std::cout << "relative path: " << filePath.string().substr(rootPath.length()) << std::endl;
        // std::cout << "filename: " << filePath.filename().string() << ", extension: " << filePath.extension().string() << std::endl;
        if (urlPath == "/index.html")
            urlPath = "/";
        handlersMap[urlPath] = {HTTPMethod::GET, std::bind(&SendStaticFile, absolutePath, _1, _2)};
    }
    return handlersMap;
}

static void HandlePostForm(HTTPRequest *req, HTTPResponse *res) {
    static int inc = 0;
    // auto filename = req.content_filename();
    auto filename = req->get_header("filename");
    if (!filename.empty()) {
        std::ofstream outputFile(libs::simple_format("post-file/{}-{}", filename, ++inc));
        if (outputFile.is_open()) {
            outputFile << req->_body.str();
        }
    }
    std::string sendData = R"JSON({"results": "upload form successfully"})JSON";
    // res._version = req._version;
    res->insert_header({"Content-Type", "application/json"});
    res->status_code(HTTPStatusCode::OK);
    res->set_str_body(sendData);
}

using namespace std::string_literals;
// using std::cout, std::endl;
// using std::string, std::map, std::tuple;

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

#include <signal.h>
#include "libs/OSUtils.h"
extern "C" void signalHandler(int signum) {
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: [executable] <address> <port> \n";
        return -1;
    }
#ifdef DEBUG
    printf("----------------- DEBUG BUILD -----------------\n");
#endif
    std::string address(argv[1]);
    auto port = stoi(std::string{argv[2]});

    SimpleServer server(address, port, 4);
    server.addHandlers({
            {"/", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/index.html", _1, _2)}},
            // {"/[a-zA-z0-9_-].+", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/index.html", _1, _2)}},
            {"/styles.css", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/styles.css", _1, _2)}},
            {"^/(simple)?test$", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/index-backup.html", _1, _2)}},
            {"/file", {HTTPMethod::POST, &HandlePostForm}},
    });
    auto staticResMap = ServeStaticResources("static");
    server.addHandlers(staticResMap);
    server.start();
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGQUIT, signalHandler);
    std::atomic_bool stopPrint{false};
    std::thread statPrinter([&stopPrint, &server]() {
        while (!stopPrint.load(std::memory_order_acquire)) {
            printf("Active conn size: %d\n", server.getActiveConnStat());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    libs::waitForTerminationRequest();
    stopPrint.store(true);
    server.stop();
    statPrinter.join();
    return 0;
}

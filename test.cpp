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

#include "libs/http/HttpMessage.h"
#ifdef USE_OPENSSL
#include "libs/SSLUtils.h"
#endif
#include "libs/http/SimpleServer.h"

#include "libs/StrUtils.h"

using namespace std::placeholders;
using namespace libs::http;
using namespace std::string_literals;
// using std::cout, std::endl;
// using std::string, std::map, std::tuple;

constexpr auto g_defaultAddr = "0.0.0.0";
constexpr auto g_defaultPort = 11225;
constexpr auto g_workerSize = 4;
constexpr size_t g_maxFD = 65535;

std::shared_ptr<SimpleServer> server_{nullptr};

#include <signal.h>
#include "libs/OSUtils.h"
extern "C" void signalHandler(int signum) {
}

struct Opts {
    Opts() : addr(g_defaultAddr), port(g_defaultPort), workerSize(g_workerSize) {}

    std::string addr;
    int port;
    int workerSize;
};

void onResourceNotFound(HTTPRequest *req, HTTPResponse *res) {
    res->http_code(CODE_404);
    res->insert_header({"Content-Type", "application/json"});
    auto sendData = R"JSON({"errors": "Not Found"})JSON";
    res->str_body(sendData);
}

static void sendStaticFile(std::string path, HTTPRequest *req, HTTPResponse *response) {
    // if (!fs::exists(path)) {
    //     // send 404 not found
    //     std::string sendData = R"JSON({"errors": "resource not found"})JSON";
    //     response->http_code(CODE_404);
    //     response->str_body(sendData);
    //     response->insert_header({"Content-Type", "application/json"});
    //     return;
    // }

    auto extension = fs::path(path).extension().string();
    if (libs::http::_mimeTypes.count(extension)) {
        response->insert_header({"Content-Type", libs::http::_mimeTypes.at(extension)});
    } else {
        response->insert_header({"Content-Type", "application/octet-stream"});// default for others
    }
    response->http_code(CODE_200);
    // std::stringstream ss;
    // ss << file.rdbuf();
    // response->str_body(ss.str());
    response->file_body(path);
    // todo: leaked mem due to connection not being clean up (tested by chrome)
}

std::vector<std::tuple<HTTPMethod, SimpleServer::URLFormat, SimpleServer::HandlerFunction>> buildStaticAssets(std::string rootPath) {
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        return {};
    }
    std::vector<std::tuple<HTTPMethod, SimpleServer::URLFormat, SimpleServer::HandlerFunction>> ret;
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
        ret.emplace_back(HTTPMethod::GET, urlPath, [path = absolutePath](auto *req, auto *res) {
            sendStaticFile(path, req, res);
        });
    }
    return ret;
}

#include "libs/RandomUtils.h"
static void HandlePostForm(HTTPRequest *req, HTTPResponse *res) {
    static int inc = 0;
    // auto filename = req.content_filename();
    auto filename = req->get_header("filename");
    if (filename.empty()) filename = libs::random_str(5);
    if (!filename.empty()) {
        std::ofstream outputFile(libs::simple_format("post-file/{}-{}", filename, ++inc));
        if (outputFile.is_open()) {
            outputFile << req->body_str();
        }
    }
    std::string sendData = R"JSON({"results": "upload form successfully"})JSON";
    res->insert_header({"Content-Type", "application/json"});
    res->http_code(CODE_200);
    res->str_body(sendData);
}

#include "json11/json11.hpp"

static void HandleGetStat(HTTPRequest *req, HTTPResponse *res) {
    json11::Json::object stat{};
    if (!server_) {
        res->http_code(CODE_500);
        return;
    }
    auto metrics = server_->getStat();
    stat["active_conn"] = metrics.activeConn;
    stat["success_req"] = static_cast<int>(metrics.successReq);
    stat["failed_req"] = metrics.failedReq;
    stat["cache_conn"] = metrics.cacheConn;
    stat["created_conn"] = metrics.createdConn;
    stat["dropped_conn"] = metrics.dropConn;
    res->insert_header({"Content-Type", "application/json"});
    res->http_code(CODE_200);
    res->str_body(json11::Json(std::move(stat)).dump());
}

Opts parseOpts(int argc, const char *argv[]) {
    try {
        if (argc <= 1) return {};
        Opts opts;
        std::string arg1{argv[1]};
        auto pos = arg1.find(':');
        if (pos == std::string::npos) {
            opts.port = stoi(arg1);
            return opts;
        } else {
            opts.port = stoi(arg1.substr(pos + 1));
            opts.addr = pos == 0 ? "0.0.0.0" : arg1.substr(0, pos);
        }
        if (argc >= 3) {
            opts.workerSize = stoi(std::string{argv[2]});
        }
        return opts;
    } catch (const std::exception &e) {
        std::cerr << "exception: " << e.what() << std::endl;
        std::cerr << "Usage: [executable] <address:port> <thread_pool_size>" << std::endl;
        exit(-1);
    }
}

int main(int argc, const char *argv[]) {
#ifdef DEBUG
    printf("----------------- DEBUG BUILD -----------------\n");
#endif
    libs::setMaxFD(g_maxFD);
    auto opts = parseOpts(argc, argv);
    server_.reset(new SimpleServer(opts.addr, opts.port, opts.workerSize));
    server_->setDefaultHandler(HTTPMethod::GET, onResourceNotFound);
    server_->addHandlers({
            {HTTPMethod::GET, "/", [](auto *req, auto *res) { sendStaticFile("static/index.html", req, res); }},
            // {HTTPMethod::GET, "/styles.css", [](auto *req, auto *res) { sendStaticFile("static/styles.css", req, res); }},
            // {HTTPMethod::GET, "^/(simple)?test$", [](auto *req, auto *res) { sendStaticFile("static/index-backup.html", req, res); }},
            {HTTPMethod::POST, "/file", &HandlePostForm},
            {HTTPMethod::GET, "/stat", &HandleGetStat},
    });
    server_->addHandlers(buildStaticAssets("static"));
    server_->start();
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGQUIT, signalHandler);
    libs::waitForTerminationRequest();
    server_->stop();
    server_.reset();
    return 0;
}

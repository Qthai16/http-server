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
using namespace std::string_literals;
// using std::cout, std::endl;
// using std::string, std::map, std::tuple;

constexpr auto g_defaultAddr = "0.0.0.0";
constexpr auto g_defaultPort = 11225;
constexpr auto g_workerSize = 4;

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

static void SendStaticFile(std::string path, HTTPRequest *req, HTTPResponse *response) {
    // response->insert_header({"Connection", "keep-alive"});
    if (!fs::exists(path)) {
        // send 404 not found
        std::string sendData = R"JSON({"errors": "resource not found"})JSON";
        response->status_code(HTTPStatusCode::NotFound);
        response->str_body(sendData);
        response->insert_header({"Content-Type", "application/json"});
        return;
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        // send internal error
        std::string sendData = R"JSON({"errors": "failed to open file"})JSON";
        response->status_code(HTTPStatusCode::InternalServerError);
        response->str_body(sendData);
        response->insert_header({"Content-Type", "application/json"});
        return;
    }
    // file.close();// close here bc HTTPResponse will open this file

    auto extension = fs::path(path).extension().string();
    if (simple_http::_mimeTypes.count(extension)) {
        response->insert_header({"Content-Type", simple_http::_mimeTypes.at(extension)});
    } else {
        response->insert_header({"Content-Type", "application/octet-stream"});// default for others
    }
    response->status_code(HTTPStatusCode::OK);
    // std::stringstream ss;
    // ss << file.rdbuf();
    // response->str_body(ss.str());
    response->file_body(path);
    // todo: leaked mem due to connection not being clean up
    // response->set_file_body(path);
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
    res->insert_header({"Content-Type", "application/json"});
    res->status_code(HTTPStatusCode::OK);
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
    auto opts = parseOpts(argc, argv);
    server_.reset(new SimpleServer(opts.addr, opts.port, opts.workerSize));
    server_->addHandlers({
            {"/", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/index.html", _1, _2)}},
            // {"/[a-zA-z0-9_-].+", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/index.html", _1, _2)}},
            {"/styles.css", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/styles.css", _1, _2)}},
            {"^/(simple)?test$", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/index-backup.html", _1, _2)}},
            {"/file", {HTTPMethod::POST, &HandlePostForm}},
            {"/stat", {HTTPMethod::GET, &HandleGetStat}},
    });
    auto staticResMap = ServeStaticResources("static");
    server_->addHandlers(staticResMap);
    server_->start();
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGQUIT, signalHandler);
    libs::waitForTerminationRequest();
    server_->stop();
    server_.reset();
    return 0;
}

#include <iostream>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <netinet/in.h>
#include <regex>
#include <sstream>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include <chrono>
#include <filesystem>

#include "src/HttpMessage.h"
#ifdef USE_OPENSSL
#include "src/SSLUtils.h"
#endif
#include "src/SimpleServer.h"

using namespace HttpMessage;
using namespace std::placeholders;

#define THREADPOOL_SIZE 4
namespace fs = std::filesystem;

static void SendStaticFile(std::string path, int clientfd, const HttpMessage::HTTPRequest& req, HttpMessage::HTTPResponse& response) {
  response._version = req._version;
  response._headers["Connection"] = "keep-alive";

  if(!fs::exists(path)) {
    // send 404 not found
    std::string sendData = R"JSON({"errors": "resource not found"})JSON";
    response.status_code(HTTPStatusCode::NotFound);
    response.set_str_body(sendData);
    response._headers = {{
        {"Content-Type", "application/json"},
    }};
    return;
  }
  std::ifstream file(path);
  if(!file.is_open()) {
    // send internal error
    std::string sendData = R"JSON({"errors": "failed to open file"})JSON";
    response.status_code(HTTPStatusCode::InternalServerError);
    response.set_str_body(sendData);
    response._headers = {{
        {"Content-Type", "application/json"},
    }};
    return;
  }
  file.close(); // close here bc HTTPResponse will open this file

  auto extension = fs::path(path).extension().string();
  if(SimpleServer::_mimeTypes.count(extension)) {
    response._headers["Content-Type"] = SimpleServer::_mimeTypes.at(extension);
  }
  else {
    response._headers["Content-Type"] = "application/octet-stream"; // default for others
  }
  response.status_code(HTTPStatusCode::OK);
  response.set_file_body(path);
}

static SimpleServer::HandlersMap ServeStaticResources(std::string rootPath) {
  using namespace std::placeholders;
  SimpleServer::HandlersMap handlersMap;
  for(auto& entry : fs::recursive_directory_iterator(rootPath)) {
    if(!fs::is_regular_file(entry.path()))
      continue;
    auto filePath = entry.path();
    auto urlPath = filePath.string().substr(rootPath.length());
    auto absolutePath = filePath.string();
    // std::cout << "relative path: " << filePath.string().substr(rootPath.length()) << std::endl;
    // std::cout << "filename: " << filePath.filename().string() << ", extension: " << filePath.extension().string() << std::endl;
    if(urlPath == "/index.html")
      urlPath = "/";
    handlersMap[urlPath] = {HTTPMethod::GET, std::bind(&SendStaticFile, absolutePath, _1, _2, _3)};
  }
  return handlersMap;
}

static void HandlePostForm(int clientfd, const HttpMessage::HTTPRequest& req, HttpMessage::HTTPResponse& res) {
  std::string sendData = R"JSON({"results": "upload form successfully"})JSON";
  auto len = sendData.length();
  res._version = req._version;
  res.status_code(HTTPStatusCode::OK);
  res.set_str_body(sendData);
  res._headers = {{{"Content-Length", std::to_string(len)},
                   {"Connection", "keep-alive"}}};
  res._headers["Content-Type"] = "application/json";
  // save body data to file
  // return res;
}

// void SendResourceNotFound(int clientfd) {
//   HTTPResponse response(clientfd);
//   std::string sendData = R"JSON({"errors": "resource not found"})JSON";
//   response.status_code(HTTPStatusCode::NotFound).body(sendData);
//   response._headers = HeadersMap{{{"Content-Type", "application/json"},
//                                   {"Connection", "keep-alive"}}};
// }

using namespace std::string_literals;
using std::cout, std::endl;
using std::string, std::map;
using namespace Utils;

auto g_test_request = R"TEST(GET / HTTP/1.1
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
Accept-Encoding: gzip, deflate
Accept-Language: en-US,en;q=0.9
Cache-Control: max-age=0
Connection: keep-alive
Host: 172.31.234.35:11225
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36
)TEST";

int main(int argc, char* argv[]) {
  // test, should be split to individual files
  cout << "========== Start testing logic ==========" << endl;

  auto test_cases = map<string, string>{
      {"test_case_1", "Normal case: {}, {}"},
      {"test_case_2", "More args than placeholder {}, {}, {}"},
      {"test_case_3", "More placeholder than args {}, {}, {}, {}, {}"},
      {"test_case_4", "No placeholder"},
  };
  for(const auto& [test_case, format] : test_cases) {
    cout << simple_format("{}, format: {}", test_case, format) << endl;
  }
  easy_print("Normal case: {}, {}", 1000000, 124.0);
  easy_print("More args than placeholder {}, {}, {}", "abc", "3413413"s, 22.34f, "alfkjalfd");
  easy_print("More placeholder than args {}, {}, {}, {}, {}", 100);
  easy_print("No placeholder", 124, 22.34, 32, "alfkjalfd");
  easy_print("No placeholder, no args");

  std::stringstream ss(g_test_request);
  auto& logStream = std::cout;
  auto httpReq = std::make_unique<HTTPRequest>();
  httpReq->parse_request(ss);
  // httpReq->to_string();
  httpReq->to_json(logStream);
  logStream << endl;

  cout << "========== Done testing logic ==========" << endl;

  if(argc != 3) {
    std::cerr << "Usage: [executable] <address> <port> \n";
    return -1;
  }
  std::string address(argv[1]);
  auto port = stoi(std::string{argv[2]});

  SimpleServer server(address, port, THREADPOOL_SIZE);
  // clang-format off
  server.AddHandlers({
    {"/", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/index.html", _1, _2, _3)}},
    {"/styles.css", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/styles.css", _1, _2, _3)}},
    {"^/(simple)?test$", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/index-backup.html", _1, _2, _3)}},
    {"/form", {HTTPMethod::POST, &HandlePostForm}},
    {"/abc", {HTTPMethod::GET, std::bind(&SendStaticFile, "react-build/OPSWAT.ico", _1, _2, _3)}},
    // {"/static/css/main.b52b0c83.chunk.css", {HTTPMethod::GET, std::bind(&SendStaticFile, "react-build/static/css/main.b52b0c83.chunk.css", _1, _2, _3)}}
  });
  // clang-format on
  // auto staticResMap = ServeStaticResources("react-build");
  // server.AddHandlers(staticResMap);
  server.Start();
  server.Listen();
  return 0;
}
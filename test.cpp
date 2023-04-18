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
// #include <filesystem>

#include "src/HttpMessage.h"
#include "src/SimpleServer.h"

using namespace HttpMessage;
using namespace std::placeholders;

#define THREADPOOL_SIZE 1
// namespace fs = std::filesystem;

static void HandlePostForm(int clientfd, const HttpMessage::HTTPRequest& req, HttpMessage::HTTPResponse& res) {
  std::string sendData = R"JSON({"results": "upload form successfully"})JSON";
  auto len = sendData.length();
  // HTTPResponse res(clientfd);
  res._version = req._version;
  res.status_code(HTTPStatusCode::OK);
  res.set_str_body(sendData);
  res._headers = {{{"Content-Length", std::to_string(len)},
                        {"Connection", "keep-alive"}}};
  res._headers["Content-Type"] = "application/json";
  // return res;
}

int main(int argc, char* argv[]) {
  if(argc != 3) {
    std::cerr << "Usage: [executable] <address> <port> \n";
    return -1;
  }
  std::string address(argv[1]);
  auto port = stoi(std::string{argv[2]});

  SimpleServer server(address, port, THREADPOOL_SIZE);
  server.AddHandlers({
      {"/", {HTTPMethod::GET, std::bind(&SimpleServer::SendStaticFile, "static/index.html", _1, _2, _3)}},
      {"/styles.css", {HTTPMethod::GET, std::bind(&SimpleServer::SendStaticFile, "static/styles.css", _1, _2, _3)}},
      {"^/(simple)?test$", {HTTPMethod::GET, std::bind(&SimpleServer::SendStaticFile, "static/index-backup.html", _1, _2, _3)}},
      // {"/form", {HTTPMethod::POST, &HandlePostForm}},
      {"/abc", {HTTPMethod::GET, std::bind(&SimpleServer::SendStaticFile, "react-build/OPSWAT.ico", _1, _2, _3)}},
      {"/static/css/main.b52b0c83.chunk.css", {HTTPMethod::GET, std::bind(&SimpleServer::SendStaticFile, "react-build/static/css/main.b52b0c83.chunk.css", _1, _2, _3)}}
  });
  server.Start();
  server.Listen();
  return 0;
}
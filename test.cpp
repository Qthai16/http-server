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
// namespace fs = std::filesystem;

static HttpMessage::HTTPResponse HandlePostForm(int clientfd, const HttpMessage::HTTPRequest& req) {
  std::string sendData = R"JSON({"results": "upload form successfully"})JSON";
  auto len = sendData.length();
  HTTPResponse res(clientfd);
  res._version = req._version;
  res.status_code(HTTPStatusCode::OK).body(sendData);
  res._headers = {{{"Content-Length", std::to_string(len)},
                        {"Connection", "keep-alive"}}};
  res._headers["Content-Type"] = "application/json";
  return res;
}

int main(int argc, char* argv[]) {
  if(argc != 3) {
    std::cerr << "Usage: [executable] <address> <port> \n";
    return -1;
  }
  std::string address(argv[1]);
  auto port = stoi(std::string{argv[2]});

  SimpleServer server(address, port);
  server.AddHandlers({
      {"/", {HTTPMethod::GET, std::bind(&SimpleServer::SendStaticFile, "static/index.html", _1, _2)}},
      {"/styles.css", {HTTPMethod::GET, std::bind(&SimpleServer::SendStaticFile, "static/styles.css", _1, _2)}},
      {"/form", {HTTPMethod::POST, &HandlePostForm}},
  });
  server.Start();
  server.Listen();
  return 0;
}
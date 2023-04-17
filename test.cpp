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
#include "src/SimpleServer.h"

using namespace HttpMessage;
using namespace std::placeholders;
namespace fs = std::filesystem;

void SendStaticFile(std::string path, int clientfd) {
  static std::map<std::string, std::string> mimeTypes = {
      {".txt", "text/plain"},
      {".html", "text/html"},
      {".htm", "text/html"},
      {".css", "text/css"}};
  if(!fs::exists(path)) {
    // send 404 not found
    send(clientfd, std::string{}.data(), 0, 0);
    close(clientfd);
  }
  auto extension = fs::path(path).extension().string();
  std::stringstream ss;
  // std::ifstream index("static/index.html");
  std::ifstream index(path);
  HTTPResponse response(index);
  response._headers = {{{"Content-Length", std::to_string(content_length(index))},
                        {"Connection", "keep-alive"}}};
  if(mimeTypes.count(extension)) {
    response._headers["Content-Type"] = mimeTypes.at(extension);
  }
  else {
    response._headers["Content-Type"] = "application/octet-stream"; // default for others
  }
  ss << response;
  auto sendText = ss.rdbuf()->str();
  send(clientfd, sendText.data(), sendText.length(), 0);
  close(clientfd);
}

int main(int argc, char* argv[]) {
  // std::ifstream ifs("../request-chrome.txt");
  // HTTPRequest request(ifs);
  // std::cout << std::boolalpha << (HttpMessage::trim_str("      fsdflakjdfalkd   \r\n   \r\r   ") == std::string{"fsdflakjdfalkd"}) << std::endl;
  // std::cout << std::boolalpha << (HttpMessage::trim_str("\r\n").empty()) << std::endl;
  // std::cout << std::boolalpha << (HttpMessage::trim_str("123456789       \r\n") == std::string{"123456789"}) << std::endl;
  // std::cout << std::boolalpha << (HttpMessage::trim_str("\r\n         123456789") == std::string{"123456789"}) << std::endl;
  if(argc != 3) {
    std::cerr << "Usage: [executable] <address> <port> \n";
    return -1;
  }
  std::string address(argv[1]);
  auto port = stoi(std::string{argv[2]});

  SimpleServer server(address, port);
  server.AddHandlers({
      {"/", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/index.html", _1)}},
      {"/styles.css", {HTTPMethod::GET, std::bind(&SendStaticFile, "static/styles.css", _1)}},
  });
  server.Start();
  server.Listen();
  return 0;
}
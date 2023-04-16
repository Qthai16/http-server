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

#include "src/HttpMessage.h"

#define BUFFER_SIZE 4096
#define QUEUEBACKLOG 100

using namespace std::chrono;
using namespace std::chrono_literals;

using URLFormat = std::regex;
using HandlerFunction = std::function<void(int)>;
using HeadersMap = std::map<std::string, std::string>;

using namespace HttpMessage;

void* get_ip_address(struct sockaddr* sa) {
  if(sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[]) {
  std::ifstream ifs("../request-chrome.txt");
  HTTPRequest request(ifs);
  // std::cout << std::boolalpha << (HttpMessage::trim_str("      fsdflakjdfalkd   \r\n   \r\r   ") == std::string{"fsdflakjdfalkd"}) << std::endl;
  // std::cout << std::boolalpha << (HttpMessage::trim_str("\r\n").empty()) << std::endl;
  // std::map<URLFormat, HandlerFunction> handlerMaps;
  if(argc != 3) {
    std::cerr << "Usage: [executable] <address> <port> \n";
    return -1;
  }
  std::string address(argv[1]);
  auto port = stoi(std::string{argv[2]});

  std::cout << "Simple server start listening on " << address << ":" << port << std::endl;
  // int listenfd = 0, connfd = 0;
  // auto timeNow = system_clock::now();
  sockaddr_in serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  inet_pton(AF_INET, address.c_str(), &(serv_addr.sin_addr.s_addr));
  serv_addr.sin_port = htons(port);

  auto listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if(listenfd < 0) {
    throw std::runtime_error("Failed to open socket");
    return -1;
  }

  if(bind(listenfd, (sockaddr*)&serv_addr, sizeof(serv_addr))) {
    throw std::runtime_error("Failed to bind socket");
    return -1;
  }

  if(listen(listenfd, QUEUEBACKLOG) < 0) {
    throw std::runtime_error("Failed to listen socket");
    return -1;
  }

  char buffer[BUFFER_SIZE];
  memset(buffer, '0', sizeof(buffer));

  while(1) {
    sockaddr_storage connAddr;
    char s[INET6_ADDRSTRLEN];
    socklen_t connAddrSize = sizeof(connAddr);
    auto fd = accept(listenfd, (struct sockaddr*)&connAddr, &connAddrSize);
    if(fd == -1) {
      // something went wrong
      std::cerr << "Failed to accept incomming connection\n";
      continue;
    }

    inet_ntop(connAddr.ss_family, get_ip_address((struct sockaddr*)&connAddr), s, sizeof(s));
    std::cout << "Got connection from " << s << std::endl;

    // std::string requestData;
    // recv(fd, requestData.data(), sendText.length(), 0);
    ssize_t byte_count = recv(fd, buffer, sizeof(buffer), 0);
    if(byte_count > 0) {
      std::cout << "Received request: \n" << std::string(buffer, byte_count);
    }

      std::stringstream ss;
      std::ifstream index("index.html");
      HTTPResponse response(index);
      response._headers = {{{"Content-Type", "text/html"},
                            {"Content-Length", std::to_string(content_length(index))},
                            {"Connection", "keep-alive"}}};
      ss << response;
      auto sendText = ss.rdbuf()->str();
      send(fd, sendText.data(), sendText.length(), 0);
      close(fd);
      std::this_thread::sleep_for(10ms);
    }
    return 0;
  }
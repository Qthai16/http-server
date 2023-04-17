#pragma once

#include <iostream>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <filesystem>
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

#include "HttpMessage.h"

#define BUFFER_SIZE 4096
#define QUEUEBACKLOG 100

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace HttpMessage;

namespace fs = std::filesystem;

using URLFormat = std::string;
using HandlerFunction = std::function<void(int)>;
using HandlersMap = std::unordered_map<URLFormat, std::pair<HTTPMethod, HandlerFunction>>;


class SimpleServer {
private:
  std::string _address;
  unsigned int _port;
  HandlersMap _handlersMap;
  int _serverFd;
  char _buffer[BUFFER_SIZE]; // limit to 4KB request, need mechanism to handle big request
  std::atomic_bool _stop;
  std::chrono::milliseconds _sleepTimes;


private:
  void* get_ip_address(struct sockaddr* sa) {
    if(sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
  }

public:
  static inline std::map<std::string, std::string> _mimeTypes = {
      {".txt", "text/plain"},
      {".html", "text/html"},
      {".htm", "text/html"},
      {".css", "text/css"}};

public:
  SimpleServer() = default;
  ~SimpleServer() {
    close(_serverFd);
  }
  SimpleServer(std::string address, unsigned int port) : _address(address),
                                                         _port(port),
                                                         _handlersMap(),
                                                         _serverFd(-1),
                                                         _buffer(),
                                                         _stop(false),
                                                         _sleepTimes(10ms) {
    memset(_buffer, 0, sizeof(_buffer));
  }

  void Start() {
    sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, _address.c_str(), &(serv_addr.sin_addr.s_addr));
    serv_addr.sin_port = htons(_port);

    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if(_serverFd < 0) {
      throw std::runtime_error("Failed to open socket");
      return;
    }

    int enable = 1;
    if(setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
      throw std::runtime_error("Failed to set socket options");
      return;
    }

    if(bind(_serverFd, (sockaddr*)&serv_addr, sizeof(serv_addr))) {
      throw std::runtime_error("Failed to bind socket");
      return;
    }

    if(listen(_serverFd, QUEUEBACKLOG) < 0) {
      throw std::runtime_error("Failed to listen socket");
      return;
    }
    std::cout << "Simple server start listening on " << _address << ":" << _port << std::endl;
  }

  void Listen() {
    while(!_stop) {
      sockaddr_storage connAddr;
      char s[INET6_ADDRSTRLEN];
      socklen_t connAddrSize = sizeof(connAddr);
      auto clientfd = accept(_serverFd, (struct sockaddr*)&connAddr, &connAddrSize);
      if(clientfd == -1) {
        // something went wrong
        std::cerr << "Failed to accept incomming connection\n";
        continue;
      }

      inet_ntop(connAddr.ss_family, get_ip_address((struct sockaddr*)&connAddr), s, sizeof(s));
      std::cout << "Got connection from " << s << std::endl;

      memset(_buffer, 0, sizeof(_buffer));
      ssize_t byte_count = recv(clientfd, _buffer, sizeof(_buffer), 0);
      if(byte_count > 0) {
        std::stringstream ss;
        ss << _buffer;
        HTTPRequest req(ss);
        // req.to_string(); for debug
        // matching using regex
        if(_handlersMap.count(req._path) && _handlersMap.at(req._path).first == req._method) {
          // call registered handler
          _handlersMap.at(req._path).second(clientfd);
        }
      }
      else if(byte_count == 0) {
        close(clientfd);
        continue;
      }
      std::this_thread::sleep_for(_sleepTimes);
    }
  }

  void Stop() {
    _stop = true;
  }

  void AddHandlers(HandlersMap handlers) {
    for(const auto& [path, pair] : handlers) {
      _handlersMap[path] = pair;
    }
  }
  void AddHandlers(URLFormat path, HttpMessage::HTTPMethod method, HandlerFunction fn) {
    _handlersMap[path] = {method, fn};
  }

  void SendResourceNotFound(int clientfd) {
    HTTPResponse response(clientfd);
    std::string sendData = R"JSON({"errors": "resource not found"})JSON";
    response.status_code(HTTPStatusCode::NotFound).body(sendData);
    response._headers = HeadersMap{{
        {"Content-Type", "application/json"},
        {"Connection", "keep-alive"}
    }};
    response.write();
  }

  static void SendStaticFile(std::string path, int clientfd) {
    HTTPResponse response(clientfd);
    response._headers["Connection"] = "keep-alive";

    if(!fs::exists(path)) {
      // send 404 not found
      std::string sendData = R"JSON({"errors": "resource not found"})JSON";
      response.status_code(HTTPStatusCode::NotFound).body(sendData);
      response._headers = {{
          {"Content-Type", "application/json"},
      }};
      response.write();
    }
    std::ifstream file(path);
    if(!file.is_open()) {
      // send internal error
      std::string sendData = R"JSON({"errors": "failed to open file"})JSON";
      response.status_code(HTTPStatusCode::InternalServerError).body(sendData);
      response._headers = {{
          {"Content-Type", "application/json"},
      }};
      response.write();
    }

    auto extension = fs::path(path).extension().string();
    if(_mimeTypes.count(extension)) {
      response._headers["Content-Type"] = _mimeTypes.at(extension);
    }
    else {
      response._headers["Content-Type"] = "application/octet-stream"; // default for others
    }
    response.status_code(HTTPStatusCode::OK).body(extractStream(file));
    response.write();
  }
};
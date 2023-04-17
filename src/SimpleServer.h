#pragma once

#include <iostream>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <atomic>
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

#include "HttpMessage.h"

#define BUFFER_SIZE 4096
#define QUEUEBACKLOG 100

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace HttpMessage;

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

      // std::string requestData;
      // recv(fd, requestData.data(), sendText.length(), 0);

      memset(_buffer, 0, sizeof(_buffer));
      ssize_t byte_count = recv(clientfd, _buffer, sizeof(_buffer), 0);
      if(byte_count > 0) {
        std::stringstream ss;
        // std::cout << std::string(_buffer, byte_count);
        ss << _buffer;
        HTTPRequest req(ss);
        req.to_string();
        if (_handlersMap.count(req._path) && _handlersMap.at(req._path).first == req._method) {
          // call registered handler
          _handlersMap.at(req._path).second(clientfd);
        }
      }
      else if(byte_count == 0) {
        close(clientfd);
        continue;
      }

      // std::stringstream ss;
      // std::ifstream index("static/index.html");
      // HTTPResponse response(index);
      // response._headers = {{{"Content-Type", "text/html"},
      //                       {"Content-Length", std::to_string(content_length(index))},
      //                       {"Connection", "keep-alive"}}};
      // ss << response;
      // auto sendText = ss.rdbuf()->str();
      // send(clientfd, sendText.data(), sendText.length(), 0);
      // close(clientfd);
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
  void AddHandlers(URLFormat path, HttpMessage::HTTPMethod method , HandlerFunction fn) {
    _handlersMap[path] = {method, fn};
  }
};
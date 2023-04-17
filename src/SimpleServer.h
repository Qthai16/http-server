#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "HttpMessage.h"

#define BUFFER_SIZE 4096
#define QUEUEBACKLOG 100
#define MAX_EPOLL_EVENTS 800

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace HttpMessage;

namespace fs = std::filesystem;

using URLFormat = std::string;
using HandlerFunction = std::function<HttpMessage::HTTPResponse(int, const HttpMessage::HTTPRequest&)>;
using HandlersMap = std::unordered_map<URLFormat, std::pair<HTTPMethod, HandlerFunction>>;

struct EpollHandle {
  struct EventData {
    int _fd;
    std::size_t _length;
    std::size_t _cursor;
    char _eventBuffer[BUFFER_SIZE]; // limit to 4KB, need mechanism to handle big request body

    EventData() : _fd(0), _length(0), _cursor(0), _eventBuffer(){
                                                      // memset(_eventBuffer, 0, sizeof(_eventBuffer));
                                                  };
    EventData(int fd) : _fd(fd), _length(0), _cursor(0), _eventBuffer(){
                                                             // memset(_eventBuffer, 0, sizeof(_eventBuffer));
                                                         };

    // ~EventData() { close(_fd); }
    ~EventData() = default;
  };

  int _epollFd;
  epoll_event events[MAX_EPOLL_EVENTS];

  EpollHandle() : _epollFd(-1), events() {}
  ~EpollHandle() = default;

  void create_epoll() {
    _epollFd = epoll_create1(0);
    if(_epollFd == -1) {
      std::cerr << "failed to create epoll" << std::endl;
      throw std::runtime_error("failed to create epoll");
    }
  }

  void add_or_modify_fd(int clientFd, int eventType, int opt, EventData* eventData) {
    // auto eventData = new EventData(clientFd);
    epoll_event event;
    event.data.fd = clientFd;
    event.events = eventType;
    event.data.ptr = (void*)eventData;
    if(epoll_ctl(_epollFd, opt, clientFd, &event) == -1) {
      throw std::runtime_error("failed to add/modify fd");
    }
  }

  void delete_fd(int clientFd) {
    if(epoll_ctl(_epollFd, EPOLL_CTL_DEL, clientFd, nullptr) == -1) {
      throw std::runtime_error("failed to delete fd");
    }
    // close(clientFd);
  }
};


class SimpleServer {
private:
  std::string _address;
  unsigned int _port;
  HandlersMap _handlersMap;
  int _serverFd;
  std::atomic_bool _stop;
  std::chrono::milliseconds _sleepTimes;
  EpollHandle _epollHandle;
  std::thread _handleReqThread;


private:
  void* IPAddressParse(struct sockaddr* sa) {
    if(sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
  }

  void HandleClientConnections() {
    while(!_stop) {
      auto nfds = epoll_wait(_epollHandle._epollFd, _epollHandle.events, MAX_EPOLL_EVENTS, -1); // return immediately
      if(nfds <= 0) {
        std::this_thread::sleep_for(_sleepTimes);
        continue;
      }

      for(auto i = 0; i < nfds; i++) {
        // auto fd = _epollHandle.events[i].data.fd;
        auto reqEventData = reinterpret_cast<EpollHandle::EventData*>(_epollHandle.events[i].data.ptr);
        auto fd = reqEventData->_fd;
        auto eventTypes = _epollHandle.events[i].events;
        if(eventTypes == EPOLLIN || eventTypes == EPOLLOUT) {
          // normal case, recv or send
          HandleEvent(reqEventData, eventTypes);
        }
        else {
          // something went wrong
          // std::cout << "not implemented, event type: " << eventTypes << std::endl;
          _epollHandle.delete_fd(fd);
          delete reqEventData;
          reqEventData = nullptr;
        }
      }
    }
  }

  void HandleReadEvent(EpollHandle::EventData* eventDataPtr) {
    auto fd = eventDataPtr->_fd;
    ssize_t byte_count = recv(fd, eventDataPtr->_eventBuffer, sizeof(eventDataPtr->_eventBuffer), 0);
    if(byte_count > 0) {
      std::stringstream ss;
      ss << eventDataPtr->_eventBuffer;
      HTTPRequest req(ss);
      // req.to_string(); // for debug
      // matching using regex
      if(_handlersMap.count(req._path) && _handlersMap.at(req._path).first == req._method) {
        // call registered handler
        auto response = _handlersMap.at(req._path).second(fd, req); // handler must close fd on completion or error
        std::stringstream outStream;
        outStream << response;
        outStream.seekg(0, std::ios_base::end);
        auto size = outStream.tellg();
        outStream.seekg(0, std::ios_base::beg);
        auto resEventData = new EpollHandle::EventData(fd);
        outStream.read(resEventData->_eventBuffer, size);
        resEventData->_length = size;
        _epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, resEventData);
        delete eventDataPtr;
      }
    }
    else if((byte_count < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // no data available, try again
      _epollHandle.delete_fd(fd);
      auto reqEventData = new EpollHandle::EventData(fd);
      _epollHandle.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_ADD, reqEventData); // add again
    }
    else {
      // byte_count == 0 || other error
      _epollHandle.delete_fd(fd);
      close(fd);
      delete eventDataPtr;
    }
  }

  void HandleWriteEvent(EpollHandle::EventData* eventDataPtr) {
    // std::cout << "handle write event" << std::endl;
    auto fd = eventDataPtr->_fd;
    auto totalLen = eventDataPtr->_length;
    auto sendBytes = send(fd, eventDataPtr->_eventBuffer + eventDataPtr->_cursor, eventDataPtr->_length, 0);
    if(sendBytes >= 0) {
      if(sendBytes < totalLen) {
        eventDataPtr->_cursor += sendBytes;
        eventDataPtr->_length -= sendBytes;
        _epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, eventDataPtr);
      }
      else { // write complete, reuse this fd
        auto reqEventData = new EpollHandle::EventData(fd);
        _epollHandle.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, reqEventData);
        delete eventDataPtr;
        eventDataPtr = nullptr;
      }
    }
    else {
      if(errno == EAGAIN || errno == EWOULDBLOCK) { // retry
        _epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_ADD, eventDataPtr);
      }
      else {
        _epollHandle.delete_fd(fd);
        close(fd);
        delete eventDataPtr;
        eventDataPtr = nullptr;
      }
    }
  }

  void HandleEvent(EpollHandle::EventData* eventDataPtr, uint32_t eventType) {
    switch(eventType) {
    case EPOLLIN: {
      HandleReadEvent(eventDataPtr);
      break;
    }
    case EPOLLOUT: {
      HandleWriteEvent(eventDataPtr);
      break;
    }
    default: {
      std::cerr << "event " << eventType << " is not handle" << std::endl;
      throw std::logic_error("epoll event is not implemented");
      break;
    }
    }
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
    _stop = true;
    if(_handleReqThread.joinable())
      _handleReqThread.join();
    close(_serverFd);
  }

  SimpleServer(std::string address, unsigned int port) : _address(address),
                                                         _port(port),
                                                         _handlersMap(),
                                                         _serverFd(-1),
                                                         _stop(false),
                                                         _sleepTimes(10ms),
                                                         _epollHandle(),
                                                         _handleReqThread() {
  }

  void Start() {
    sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, _address.c_str(), &(serv_addr.sin_addr.s_addr));
    serv_addr.sin_port = htons(_port);

    _serverFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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
    _epollHandle.create_epoll();
    _handleReqThread = std::thread([this]() {
      HandleClientConnections();
    });
    std::cout << "Simple server start listening on " << _address << ":" << _port << std::endl;
  }

  void Listen() {
    while(!_stop) {
      sockaddr_storage connAddr;
      socklen_t connAddrSize = sizeof(connAddr);
      auto clientfd = accept4(_serverFd, (struct sockaddr*)&connAddr, &connAddrSize, SOCK_NONBLOCK);
      if(clientfd == -1) {
        // std::cerr << "Failed to accept incomming connection" << std::endl;
        continue;
      }

      // char s[INET6_ADDRSTRLEN];
      // inet_ntop(connAddr.ss_family, IPAddressParse((struct sockaddr*)&connAddr), s, sizeof(s));
      // std::cout << "Got connection from " << s << std::endl;

      // add client fd to epoll interest list
      auto reqEventData = new EpollHandle::EventData(clientfd);
      _epollHandle.add_or_modify_fd(clientfd, EPOLLIN, EPOLL_CTL_ADD, reqEventData); // leveled triggered
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

  // void SendResourceNotFound(int clientfd) {
  //   HTTPResponse response(clientfd);
  //   std::string sendData = R"JSON({"errors": "resource not found"})JSON";
  //   response.status_code(HTTPStatusCode::NotFound).body(sendData);
  //   response._headers = HeadersMap{{{"Content-Type", "application/json"},
  //                                   {"Connection", "keep-alive"}}};
  // }

  static HttpMessage::HTTPResponse SendStaticFile(std::string path, int clientfd, const HttpMessage::HTTPRequest& req) {
    HTTPResponse response(clientfd);
    response._version = req._version;
    response._headers["Connection"] = "keep-alive";

    if(!fs::exists(path)) {
      // send 404 not found
      std::string sendData = R"JSON({"errors": "resource not found"})JSON";
      response.status_code(HTTPStatusCode::NotFound).body(sendData);
      response._headers = {{
          {"Content-Type", "application/json"},
      }};
      return response;
    }
    std::ifstream file(path);
    if(!file.is_open()) {
      // send internal error
      std::string sendData = R"JSON({"errors": "failed to open file"})JSON";
      response.status_code(HTTPStatusCode::InternalServerError).body(sendData);
      response._headers = {{
          {"Content-Type", "application/json"},
      }};
      return response;
    }

    auto extension = fs::path(path).extension().string();
    if(_mimeTypes.count(extension)) {
      response._headers["Content-Type"] = _mimeTypes.at(extension);
    }
    else {
      response._headers["Content-Type"] = "application/octet-stream"; // default for others
    }
    response.status_code(HTTPStatusCode::OK).body(extractStream(file));
    return response;
  }
};
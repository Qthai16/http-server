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
#include "Utils.h"

#define BUFFER_SIZE 4096
#define QUEUEBACKLOG 100
#define MAX_EPOLL_EVENTS 800

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace HttpMessage;

namespace fs = std::filesystem;

struct EpollHandle {
  struct EventData {
    int _fd;
    std::size_t _length;
    std::size_t _cursor;
    char _eventBuffer[BUFFER_SIZE]; // limit to 4KB, need mechanism to handle big request body

    EventData() :
        _fd(0), _length(0), _cursor(0), _eventBuffer(){
                                            // memset(_eventBuffer, 0, sizeof(_eventBuffer));
                                        };
    EventData(int fd) :
        _fd(fd), _length(0), _cursor(0), _eventBuffer(){
                                             // memset(_eventBuffer, 0, sizeof(_eventBuffer));
                                         };

    // ~EventData() { close(_fd); }
    ~EventData() = default;
  };

  int _epollFd;
  epoll_event events[MAX_EPOLL_EVENTS];

  EpollHandle() :
      _epollFd(-1), events() {}
  ~EpollHandle() = default;

  void create_epoll() {
    _epollFd = epoll_create1(0);
    if(_epollFd == -1) {
      std::cerr << "failed to create epoll" << std::endl;
      throw std::runtime_error("failed to create epoll");
    }
  }

  void add_or_modify_fd(int clientFd, int eventType, int opt, EventData* eventData) {
    if(eventData == nullptr) {
      throw std::runtime_error("event data is null");
      return;
    }
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
public:
  using URLFormat = std::string;
  using HandlerFunction = std::function<HttpMessage::HTTPResponse(int, const HttpMessage::HTTPRequest&)>;
  using HandlersMap = std::unordered_map<URLFormat, std::pair<HTTPMethod, HandlerFunction>>;

private:
  std::string _address;
  unsigned int _port;
  std::size_t _poolSize;
  HandlersMap _handlersMap;
  int _serverFd;
  std::atomic_bool _stop;
  std::chrono::milliseconds _sleepTimes;
  std::vector<EpollHandle> _epollHandles;
  std::vector<std::thread> _workersPool;

private:
  void* IPAddressParse(struct sockaddr* sa) {
    if(sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
  }

  HandlersMap::const_iterator get_registered_path(std::string path) {
    // matching using regex, slower than string exact match
    auto iter = std::find_if(_handlersMap.cbegin(), _handlersMap.cend(), [path](const HandlersMap::value_type& pair) {
      auto pathRegex = std::regex(pair.first);
      return std::regex_match(path, pathRegex);
    });
    return iter;
  }

  void HandleClientConnections(EpollHandle& epollHandle) {
    while(!_stop) {
      auto nfds = epoll_wait(epollHandle._epollFd, epollHandle.events, MAX_EPOLL_EVENTS, -1); // wait forever
      if(nfds <= 0) {
        std::this_thread::sleep_for(_sleepTimes);
        continue;
      }

      for(auto i = 0; i < nfds; i++) {
        // sometimes EPOLLHUP and EPOLLERR bit is set make fd a not valid value
        // auto fd = epollHandle.events[i].data.fd;
        auto reqEventData = reinterpret_cast<EpollHandle::EventData*>(epollHandle.events[i].data.ptr);
        auto fd = reqEventData->_fd;
        auto eventTypes = epollHandle.events[i].events;
        if(eventTypes == EPOLLIN || eventTypes == EPOLLOUT) {
          // normal case, recv or send
          HandleEvent(epollHandle, reqEventData, eventTypes);
        }
        else {
          // errors or event is not handled
          epollHandle.delete_fd(fd);
          close(fd);
          delete reqEventData;
          reqEventData = nullptr;
        }
      }
    }
  }

  void HandleReadEvent(EpollHandle& epollHandle, EpollHandle::EventData* eventDataPtr) {
    auto fd = eventDataPtr->_fd;
    auto byte_count = recv(fd, eventDataPtr->_eventBuffer, sizeof(eventDataPtr->_eventBuffer), 0);
    if(byte_count > 0) {
      std::stringstream ss;
      ss << eventDataPtr->_eventBuffer;
      HTTPRequest req(ss);
      // req.to_string(); // for debug
      auto maybeIter = get_registered_path(req._path);
      if(maybeIter != _handlersMap.end() && maybeIter->second.first == req._method) {
        // call registered handler
        auto response = maybeIter->second.second(fd, req); // handler must close fd on completion or error
        std::stringstream outStream;
        outStream << response;
        outStream.seekg(0, std::ios_base::end);
        auto size = outStream.tellg();
        outStream.seekg(0, std::ios_base::beg);
        auto resEventData = new EpollHandle::EventData(fd);
        outStream.read(resEventData->_eventBuffer, size);
        resEventData->_length = size;
        epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, resEventData);
        delete eventDataPtr;
      }
    }
    else if((byte_count < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // no data available, try again
      epollHandle.delete_fd(fd);
      auto reqEventData = new EpollHandle::EventData(fd);
      epollHandle.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_ADD, reqEventData);
    }
    else {
      // byte_count = 0 (connection close by client) and other errors
      epollHandle.delete_fd(fd);
      close(fd);
      delete eventDataPtr;
    }
  }

  void HandleWriteEvent(EpollHandle& epollHandle, EpollHandle::EventData* eventDataPtr) {
    auto fd = eventDataPtr->_fd;
    auto totalLen = eventDataPtr->_length;
    auto sendBytes = send(fd, eventDataPtr->_eventBuffer + eventDataPtr->_cursor, eventDataPtr->_length, 0);
    if(sendBytes >= 0) {
      if(sendBytes < totalLen) {
        eventDataPtr->_cursor += sendBytes;
        eventDataPtr->_length -= sendBytes;
        epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, eventDataPtr);
      }
      else { // write complete, reuse this fd for read event
        auto reqEventData = new EpollHandle::EventData(fd);
        epollHandle.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, reqEventData);
        // epollHandle.delete_fd(fd);
        delete eventDataPtr;
        eventDataPtr = nullptr;
      }
    }
    else {
      if(errno == EAGAIN || errno == EWOULDBLOCK) { // retry
        epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_ADD, eventDataPtr);
      }
      else {
        epollHandle.delete_fd(fd);
        close(fd);
        delete eventDataPtr;
        eventDataPtr = nullptr;
      }
    }
  }

  void HandleEvent(EpollHandle& epollHandle, EpollHandle::EventData* eventDataPtr, uint32_t eventType) {
    switch(eventType) {
    case EPOLLIN: {
      HandleReadEvent(epollHandle, eventDataPtr);
      break;
    }
    case EPOLLOUT: {
      HandleWriteEvent(epollHandle, eventDataPtr);
      break;
    }
    default: {
      std::cerr << "event " << eventType << " is not handle" << std::endl;
      throw std::logic_error("event is not handled");
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
    for(auto& t : _workersPool) {
      t.join();
    }
    close(_serverFd);
  }

  SimpleServer(std::string address, unsigned int port, std::size_t poolSize = 1) :
      _address(address),
      _port(port),
      _poolSize(poolSize),
      _handlersMap(),
      _serverFd(-1),
      _stop(false),
      _sleepTimes(10ms),
      _epollHandles(),
      _workersPool() {
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

    for(std::size_t i = 0; i < _poolSize; ++i) {
      auto handle = EpollHandle();
      handle.create_epoll();
      _epollHandles.push_back(std::move(handle));
      _workersPool.push_back(std::thread(std::bind(&SimpleServer::HandleClientConnections, this, _epollHandles.back())));
    }
    std::cout << "Simple server start listening on " << _address << ":" << _port << std::endl;
  }

  void Listen() {
    std::size_t spinCnt = 0;
    while(!_stop) {
      sockaddr_storage connAddr;
      socklen_t connAddrSize = sizeof(connAddr);
      auto clientfd = accept4(_serverFd, (struct sockaddr*)&connAddr, &connAddrSize, SOCK_NONBLOCK);
      if(clientfd == -1) {
        // std::cerr << "Failed to accept incomming connection" << std::endl;
        // std::this_thread::sleep_for(_sleepTimes);
        continue;
      }

      // char s[INET6_ADDRSTRLEN];
      // inet_ntop(connAddr.ss_family, IPAddressParse((struct sockaddr*)&connAddr), s, sizeof(s));
      // std::cout << "Got connection from " << s << std::endl;

      // add fd to epoll interest list
      auto reqEventData = new EpollHandle::EventData(clientfd);
      _epollHandles[spinCnt].add_or_modify_fd(clientfd, EPOLLIN, EPOLL_CTL_ADD, reqEventData); // leveled triggered
      if(++spinCnt >= _poolSize)
        spinCnt = 0;
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
    response.status_code(HTTPStatusCode::OK).body(Utils::extract_stream(file));
    return response;
  }
};
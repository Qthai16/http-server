#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
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

struct EpollHandle {
  struct EventData {
    int _fd;
    char _eventBuffer[BUFFER_SIZE];
    HTTPRequest* _request;
    HTTPResponse* _response;

    EventData() :
        _fd(0), _eventBuffer(), _request(new HTTPRequest()), _response(nullptr){};
    EventData(int fd) :
        _fd(fd), _eventBuffer(), _request(new HTTPRequest()), _response(new HTTPResponse(fd)){};

    // ~EventData() { close(_fd); }
    ~EventData() {
      if(_request)
        delete _request;
      if(_response)
        delete _response;
    };
  };

  int _epollFd;
  epoll_event events[MAX_EPOLL_EVENTS];

  EpollHandle() :
      _epollFd(-1), events() {}
  ~EpollHandle() {
    // close(_epollFd);
  }

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
      std::cout << "failed to add/modify fd: " << strerror(errno) << std::endl;
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
  using HandlerFunction = std::function<void(int, const HttpMessage::HTTPRequest&, HttpMessage::HTTPResponse&)>;
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

  HandlersMap::const_iterator get_registered_path(std::string path, bool exactMatch = true) {
    if(exactMatch)
      return _handlersMap.find(path);
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
    if(eventDataPtr == nullptr) {
      throw std::logic_error("sth went wrong with your logic, event data is nullptr");
      return;
    }
    auto httpReqPtr = eventDataPtr->_request;
    auto byte_count = recv(fd, eventDataPtr->_eventBuffer, sizeof(eventDataPtr->_eventBuffer), 0);
    if(byte_count > 0) {
      httpReqPtr->_bufferStream.write(eventDataPtr->_eventBuffer, byte_count);
      httpReqPtr->parse_request();
      if(httpReqPtr->_totalRead < httpReqPtr->content_length()) { // still have bytes to read
        epollHandle.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, eventDataPtr);
      }
      else { // read done
        auto matchIter = get_registered_path(httpReqPtr->_path); // exact match first
        if(matchIter == _handlersMap.end()) {
          matchIter = get_registered_path(httpReqPtr->_path, false);
        }
        if(matchIter != _handlersMap.end() && matchIter->second.first == httpReqPtr->_method) {
          // call registered handler
          auto resEventData = new EpollHandle::EventData(fd);
          auto httpResPtr = resEventData->_response;
          matchIter->second.second(fd, *httpReqPtr, *httpResPtr); // handler must close fd on completion or error
          // serialize first part of response, the rest of body will be handle in HandleWriteEvent
          httpResPtr->serialize_reponse(resEventData->_eventBuffer, BUFFER_SIZE);
          epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, resEventData);
          delete eventDataPtr;
        }
        // handle not registered path or method here
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
    auto sendBytes = send(fd, eventDataPtr->_eventBuffer, sizeof(eventDataPtr->_eventBuffer), 0);
    if(sendBytes >= 0) {
      if(eventDataPtr->_response->_totalWrite < eventDataPtr->_response->_contentLength) { // still have bytes to send
        eventDataPtr->_response->serialize_reponse(eventDataPtr->_eventBuffer, BUFFER_SIZE);
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
      {".js", "text/javascript"},
      {".txt", "text/plain"},
      {".html", "text/html"},
      {".htm", "text/html"},
      {".svg", "image/svg+xml"},
      {".png", "image/png"},
      {".jpg", "image/jpeg"},
      {".jpeg", "image/jpeg"},
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
        std::this_thread::sleep_for(_sleepTimes); // reduce CPU usage
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
};

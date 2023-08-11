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
    std::size_t _bytesInBuffer;
    HTTPRequest* _request;
    HTTPResponse* _response;

    EventData() :
        _fd(0), _eventBuffer(), _bytesInBuffer(BUFFER_SIZE), _request(nullptr), _response(nullptr){};
    EventData(int fd) :
        _fd(fd), _eventBuffer(), _bytesInBuffer(BUFFER_SIZE), _request(new HTTPRequest()), _response(new HTTPResponse()){};

    // ~EventData() { close(_fd); }
    ~EventData() {
      if(_request)
        delete _request;
      if(_response)
        delete _response;
    };

    void clear_buffer() {
      memset(&_eventBuffer, 0, sizeof(_eventBuffer));
    }
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
  void* IPAddressParse(struct sockaddr* sa);
  HandlersMap::const_iterator get_registered_path(std::string path, bool exactMatch = true);
  void HandleClientConnections(EpollHandle& epollHandle);
  void EventLoop(EpollHandle& epollHandle, EpollHandle::EventData* eventDataPtr, uint32_t eventType);
  void HandleReadEvent(EpollHandle& epollHandle, EpollHandle::EventData* eventDataPtr);
  void HandleWriteEvent(EpollHandle& epollHandle, EpollHandle::EventData* eventDataPtr);

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
  ~SimpleServer();
  SimpleServer(std::string address, unsigned int port, std::size_t poolSize = 1);

  void Start();
  void Listen();
  void Stop();

  void AddHandlers(HandlersMap handlers);
  void AddHandlers(URLFormat path, HttpMessage::HTTPMethod method, HandlerFunction fn);
};

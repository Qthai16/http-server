#include "SimpleServer.h"

SimpleServer::~SimpleServer() {
  _stop = true;
  for(auto& t : _workersPool) {
    t.join();
  }
  close(_serverFd);
}

SimpleServer::SimpleServer(std::string address, unsigned int port, std::size_t poolSize) :
    _address(address),
    _port(port),
    _poolSize(poolSize),
    _handlersMap(),
    _serverFd(-1),
    _stop(false),
    _sleepTimes(10ms),
    _epollHandles(),
    _workersPool()
{

}

void SimpleServer::Start() {
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

void SimpleServer::Stop() {
  _stop = true;
}

void SimpleServer::Listen() {
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

void SimpleServer::AddHandlers(HandlersMap handlers) {
  for(const auto& [path, pair] : handlers) {
    _handlersMap[path] = pair;
  }
}
void SimpleServer::AddHandlers(URLFormat path, HttpMessage::HTTPMethod method, HandlerFunction fn) {
  _handlersMap[path] = {method, fn};
}

void* SimpleServer::IPAddressParse(struct sockaddr* sa) {
  if(sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

SimpleServer::HandlersMap::const_iterator SimpleServer::get_registered_path(std::string path, bool exactMatch) {
  if(exactMatch)
    return _handlersMap.find(path);
  // matching using regex, slower than string exact match
  auto iter = std::find_if(_handlersMap.cbegin(), _handlersMap.cend(), [path](const HandlersMap::value_type& pair) {
    auto pathRegex = std::regex(pair.first);
    return std::regex_match(path, pathRegex);
  });
  return iter;
}

void SimpleServer::HandleClientConnections(EpollHandle& epollHandle) {
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
        EventLoop(epollHandle, reqEventData, eventTypes);
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

void SimpleServer::EventLoop(EpollHandle& epollHandle, EpollHandle::EventData* eventDataPtr, uint32_t eventType) {
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

void SimpleServer::HandleReadEvent(EpollHandle& epollHandle, EpollHandle::EventData* eventDataPtr) {
  auto fd = eventDataPtr->_fd;
  if(eventDataPtr == nullptr) {
    throw std::logic_error("sth went wrong with your logic, event data is nullptr");
    return;
  }
  auto httpReqPtr = eventDataPtr->_request;
  auto byte_count = recv(fd, eventDataPtr->_eventBuffer, sizeof(eventDataPtr->_eventBuffer), 0);
  if(byte_count > 0) {
    httpReqPtr->parse_request(httpReqPtr->_bufferStream, eventDataPtr->_eventBuffer, byte_count);
    if(httpReqPtr->_expectContinue) {
      auto resEventData = new EpollHandle::EventData(fd);
      std::stringstream ss;
      Utils::format_impl(ss, "{} {} {}\r\n", version_str(httpReqPtr->_version),
        std::to_string(HTTPStatusCode::Continue),
        status_code_str(HTTPStatusCode::Continue));
      ss << "\r\n";
      auto text = ss.str();
      text.copy(resEventData->_eventBuffer, text.size());
      epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, resEventData);
      send(fd, resEventData->_eventBuffer, resEventData->_bytesInBuffer, 0);
      httpReqPtr->_expectContinue = false;
      delete resEventData;
    }
    if(httpReqPtr->_totalRead < httpReqPtr->content_length()) { // still have bytes to read
      epollHandle.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, eventDataPtr);
    }
    else { // read done
      auto httpResPtr = eventDataPtr->_response;
      eventDataPtr->clear_buffer();
      auto matchIter = get_registered_path(httpReqPtr->_path); // exact match first
      if(matchIter == _handlersMap.end()) {
        matchIter = get_registered_path(httpReqPtr->_path, false);
      }
      if(matchIter != _handlersMap.end() && matchIter->second.first == httpReqPtr->_method) {
        matchIter->second.second(*httpReqPtr, *httpResPtr); // call registered handler
        // serialize first part of response, the rest of body will be handle in HandleWriteEvent
        eventDataPtr->_bytesInBuffer = httpResPtr->serialize_reponse(eventDataPtr->_eventBuffer, BUFFER_SIZE);
        epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, eventDataPtr);
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

void SimpleServer::HandleWriteEvent(EpollHandle& epollHandle, EpollHandle::EventData* eventDataPtr) {
  auto fd = eventDataPtr->_fd;
  auto sendBytes = send(fd, eventDataPtr->_eventBuffer, eventDataPtr->_bytesInBuffer, 0);
  if(sendBytes >= 0) {
    if(eventDataPtr->_response->_totalWrite < eventDataPtr->_response->_contentLength) { // still have bytes to send
      eventDataPtr->_bytesInBuffer = eventDataPtr->_response->serialize_reponse(eventDataPtr->_eventBuffer, BUFFER_SIZE);
      epollHandle.add_or_modify_fd(fd, EPOLLOUT, EPOLL_CTL_MOD, eventDataPtr);
    }
    else { // write complete
      // if keep alive, reuse this fd for read event, else close it
      // auto reqEventData = new EpollHandle::EventData(fd);
      // epollHandle.add_or_modify_fd(fd, EPOLLIN, EPOLL_CTL_MOD, reqEventData);
      epollHandle.delete_fd(fd);
      close(fd);
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
